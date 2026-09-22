package liveops

import (
	"bytes"
	"encoding/base64"
	"encoding/json"
	"image"
	"image/color"
	"image/jpeg"
	"image/png"
	"reflect"
	"strings"
	"testing"
)

func testImage(t *testing.T, format string, width, height int) map[string]any {
	t.Helper()
	pixels := image.NewRGBA(image.Rect(0, 0, width, height))
	pixels.Set(0, 0, color.RGBA{R: 255, A: 255})
	var b bytes.Buffer
	var err error
	if format == "jpeg" {
		err = jpeg.Encode(&b, pixels, nil)
	} else {
		err = png.Encode(&b, pixels)
	}
	if err != nil {
		t.Fatal(err)
	}
	return map[string]any{"data": base64.StdEncoding.EncodeToString(b.Bytes()), "width": float64(100), "placement": "left"}
}
func TestMediaRejectsInvalidOperationsAtomically(t *testing.T) {
	pngImage := testImage(t, "png", 2, 2)
	cases := []struct {
		name, field string
		value       any
	}{
		{"image string", "image", "bad"},
		{"image invalid base64", "image", map[string]any{"data": "%%%", "width": 100}},
		{"image nonimage bytes", "image", map[string]any{"data": base64.StdEncoding.EncodeToString([]byte("not an image")), "width": 100}},
		{"image missing width", "image", map[string]any{"data": pngImage["data"]}},
		{"image zero width", "image", map[string]any{"data": pngImage["data"], "width": 0}},
		{"image oversized width", "image", map[string]any{"data": pngImage["data"], "width": 1025}},
		{"image bad placement", "image", map[string]any{"data": pngImage["data"], "width": 100, "placement": "middle"}},
		{"image dimensions", "image", testImage(t, "png", 2049, 1)},
		{"resources object", "resources", map[string]any{}},
		{"resource missing name", "resources", []any{map[string]any{"kind": "url", "target": "https://example.com"}}},
		{"resource credentials", "resources", []any{map[string]any{"kind": "url", "target": "https://user:password@example.com", "name": "secret"}}},
		{"resource no host", "resources", []any{map[string]any{"kind": "url", "target": "https:///path", "name": "empty"}}},
		{"resource scheme", "resources", []any{map[string]any{"kind": "url", "target": "javascript:alert(1)", "name": "bad"}}},
		{"resource NUL", "resources", []any{map[string]any{"kind": "file", "target": "/tmp/a\x00b", "name": "bad"}}},
		{"resource invalid escape", "resources", []any{map[string]any{"kind": "url", "target": "https://example.com/%ZZ", "name": "bad"}}},
		{"resource long name", "resources", []any{map[string]any{"kind": "url", "target": "https://example.com", "name": strings.Repeat("😀", 257)}}},
		{"resource unknown kind", "resources", []any{map[string]any{"kind": "executable", "target": "/tmp/a", "name": "bad"}}},
		{"resource count", "resources", make([]any, 101)},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			d := fixture(t)
			before, _ := json.Marshal(d)
			_, err := Apply(d, []Operation{{"set", []string{"nodes", "legacy:8", "text"}, "must roll back"}, {"set", []string{"nodes", "legacy:8", tc.field}, tc.value}})
			if err == nil {
				t.Fatal("accepted invalid media")
			}
			after, _ := json.Marshal(d)
			if !bytes.Equal(before, after) {
				t.Fatal("invalid media mutated original document")
			}
		})
	}
}
func TestValidMediaRoundTrip(t *testing.T) {
	for _, format := range []string{"png", "jpeg"} {
		t.Run(format, func(t *testing.T) {
			d := fixture(t)
			imageValue := testImage(t, format, 3, 2)
			resources := []any{map[string]any{"kind": "url", "target": "https://example.com/page?q=hello#part", "name": "Reference"}, map[string]any{"kind": "file", "target": "../notes.txt", "name": "Notes"}}
			d, err := Apply(d, []Operation{{"set", []string{"nodes", "legacy:8", "image"}, imageValue}, {"set", []string{"nodes", "legacy:8", "resources"}, resources}})
			if err != nil {
				t.Fatal(err)
			}
			out, err := Project(d)
			if err != nil {
				t.Fatal(err)
			}
			again, err := Normalize(out)
			if err != nil {
				t.Fatal(err)
			}
			if !reflect.DeepEqual(d, again) {
				t.Fatal("media roundtrip changed document")
			}
		})
	}
}
func TestImageRejectsCorruptPixelsAndAspectWidth(t *testing.T) {
	full := testImage(t, "png", 10, 20)
	raw, _ := base64.StdEncoding.DecodeString(full["data"].(string))
	full["data"] = base64.StdEncoding.EncodeToString(raw[:len(raw)-15])
	d := fixture(t)
	if _, err := Apply(d, []Operation{{"set", []string{"nodes", "legacy:8", "image"}, full}}); err == nil {
		t.Fatal("accepted truncated PNG")
	}
	full = testImage(t, "png", 10, 20)
	full["width"] = float64(513)
	if _, err := Apply(d, []Operation{{"set", []string{"nodes", "legacy:8", "image"}, full}}); err == nil {
		t.Fatal("accepted oversized portrait display width")
	}
}

func TestMediaCumulativeBudgets(t *testing.T) {
	value := testImage(t, "png", 10, 20)
	raw, _ := base64.StdEncoding.DecodeString(value["data"].(string))
	for _, budget := range []mediaBudget{{stored: documentImageLimit - int64(len(raw))}, {decoded: decodedImageLimit - 10*20*4}} {
		if err := validateMedia(map[string]any{"image": value}, &budget); err != nil {
			t.Fatalf("exact budget rejected: %v", err)
		}
		before := budget
		if err := validateMedia(map[string]any{"image": value}, &budget); err == nil {
			t.Fatal("accepted image beyond cumulative budget")
		}
		if budget != before {
			t.Fatal("failed image consumed budget")
		}
	}
}

func TestImageJPEGOrientationBounds(t *testing.T) {
	value := testImage(t, "jpeg", 20, 10)
	raw, _ := base64.StdEncoding.DecodeString(value["data"].(string))
	// EXIF IFD0 orientation 6 rotates this landscape JPEG into a portrait.
	exif := []byte{'E', 'x', 'i', 'f', 0, 0, 'I', 'I', 42, 0, 8, 0, 0, 0, 1, 0, 0x12, 1, 3, 0, 1, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0}
	withEXIF := append([]byte{}, raw[:2]...)
	withEXIF = append(withEXIF, 0xff, 0xe1, 0, byte(len(exif)+2))
	withEXIF = append(withEXIF, exif...)
	withEXIF = append(withEXIF, raw[2:]...)
	value["data"] = base64.StdEncoding.EncodeToString(withEXIF)
	value["width"] = float64(513)
	if err := validateMedia(map[string]any{"image": value}, &mediaBudget{}); err == nil {
		t.Fatal("ignored EXIF orientation width bound")
	}
	value["width"] = float64(512)
	if err := validateMedia(map[string]any{"image": value}, &mediaBudget{}); err != nil {
		t.Fatal(err)
	}
}
