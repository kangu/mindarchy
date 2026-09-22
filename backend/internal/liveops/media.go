package liveops

import (
	"bytes"
	"encoding/base64"
	"encoding/binary"
	"image"
	_ "image/jpeg"
	_ "image/png"
	"math"
	"net/url"
	"strings"
	"unicode"
	"unicode/utf16"
)

const storedImageLimit = 8 << 20
const documentImageLimit = 12 << 20
const decodedImageLimit = 128 << 20

type mediaBudget struct{ stored, decoded int64 }

// validateMedia checks portable data before it can enter a durable snapshot.
// It does not open file resources or fetch URLs.
func validateMedia(node map[string]any, budget *mediaBudget) error {
	if value, exists := node["resources"]; exists {
		if err := validateResources(value); err != nil {
			return err
		}
	}
	value, exists := node["image"]
	if !exists {
		return nil
	}
	obj, ok := object(value)
	if !ok {
		return bad("invalid embedded image object")
	}
	encoded, ok := obj["data"].(string)
	if !ok || len(encoded) > storedImageLimit*4/3+4 {
		return bad("invalid embedded image data")
	}
	raw, err := base64.StdEncoding.DecodeString(encoded)
	if err != nil || len(raw) == 0 || len(raw) > storedImageLimit {
		return bad("invalid embedded image encoding")
	}
	width, ok := number(obj["width"])
	if !ok || width <= 0 {
		return bad("invalid embedded image width")
	}
	if placement, exists := obj["placement"]; exists && !oneOf(placement, "left", "right", "top", "bottom") {
		return bad("invalid embedded image placement")
	}
	config, format, err := image.DecodeConfig(bytes.NewReader(raw))
	if err != nil || (format != "png" && format != "jpeg") || config.Width < 1 || config.Height < 1 || config.Width > 2048 || config.Height > 2048 {
		return bad("invalid embedded image dimensions or format")
	}
	imageWidth, imageHeight := config.Width, config.Height
	// Qt auto-transforms JPEG orientation before calculating the display bounds.
	if format == "jpeg" && jpegSwapsAxes(raw) {
		imageWidth, imageHeight = imageHeight, imageWidth
	}
	maxWidth := 1024 * float64(imageWidth) / math.Max(float64(imageWidth), float64(imageHeight))
	if width > maxWidth+.001 {
		return bad("embedded image display width exceeds limit")
	}
	bytesPerPixel := int64(4)
	// Qt retains 16-bit PNG channels in a 64-bit image buffer.
	if format == "png" && len(raw) > 24 && raw[24] == 16 {
		bytesPerPixel = 8
	}
	stored := budget.stored + int64(len(raw))
	decoded := budget.decoded + int64(config.Width)*int64(config.Height)*bytesPerPixel
	if stored > documentImageLimit || decoded > decodedImageLimit {
		return bad("document image memory limit exceeded")
	}
	// Decode pixels as well as the header: a valid header alone can hide corrupt
	// compressed data that Engine would reject when opening the durable snapshot.
	pixels, decodedFormat, err := image.Decode(bytes.NewReader(raw))
	if err != nil || decodedFormat != format || pixels.Bounds().Dx() != config.Width || pixels.Bounds().Dy() != config.Height {
		return bad("invalid embedded image pixels")
	}
	budget.stored, budget.decoded = stored, decoded
	return nil
}

func validateResources(value any) error {
	resources, ok := value.([]any)
	if !ok || len(resources) > 100 {
		return bad("invalid node resources")
	}
	for _, value := range resources {
		resource, ok := object(value)
		if !ok {
			return bad("invalid resource object")
		}
		kind, ko := resource["kind"].(string)
		target, to := resource["target"].(string)
		name, no := resource["name"].(string)
		if !ko || !to || !no || target == "" || len(utf16.Encode([]rune(target))) > 8192 || len(utf16.Encode([]rune(name))) > 512 || strings.ContainsRune(target, 0) {
			return bad("invalid resource fields")
		}
		if kind == "file" {
			continue
		}
		if kind != "url" {
			return bad("invalid resource kind")
		}
		parsed, err := url.Parse(target)
		if err != nil || parsed.Opaque != "" || (parsed.Scheme != "https" && parsed.Scheme != "http") || parsed.Hostname() == "" || parsed.User != nil || strings.ContainsAny(target, "\\<>{}|^") {
			return bad("invalid resource URL")
		}
		// QUrl StrictMode rejects whitespace/control characters in URLs. Go's parser
		// permits spaces in paths, so reject them explicitly instead of repairing input.
		for _, r := range target {
			if unicode.IsSpace(r) || unicode.IsControl(r) {
				return bad("invalid resource URL characters")
			}
		}
	}
	return nil
}

// jpegSwapsAxes reads only the EXIF orientation tag, with all offsets bounded.
// The JPEG decoder validates the actual image separately.
func jpegSwapsAxes(raw []byte) bool {
	if len(raw) < 2 || raw[0] != 0xff || raw[1] != 0xd8 {
		return false
	}
	for offset := 2; offset+4 <= len(raw); {
		if raw[offset] != 0xff {
			return false
		}
		for offset < len(raw) && raw[offset] == 0xff {
			offset++
		}
		if offset >= len(raw) {
			return false
		}
		marker := raw[offset]
		offset++
		if marker == 0xda || marker == 0xd9 {
			return false
		}
		if marker == 0x01 || (marker >= 0xd0 && marker <= 0xd7) {
			continue
		}
		if offset+2 > len(raw) {
			return false
		}
		length := int(binary.BigEndian.Uint16(raw[offset : offset+2]))
		if length < 2 || length > len(raw)-offset {
			return false
		}
		segment := raw[offset+2 : offset+length]
		offset += length
		if marker != 0xe1 || len(segment) < 14 || !bytes.Equal(segment[:6], []byte("Exif\x00\x00")) {
			continue
		}
		data := segment[6:]
		var order binary.ByteOrder
		if string(data[:2]) == "II" {
			order = binary.LittleEndian
		} else if string(data[:2]) == "MM" {
			order = binary.BigEndian
		} else {
			continue
		}
		if order.Uint16(data[2:4]) != 42 {
			continue
		}
		start := uint64(order.Uint32(data[4:8]))
		if start+2 > uint64(len(data)) {
			continue
		}
		count := int(order.Uint16(data[start : start+2]))
		for i := 0; i < count; i++ {
			pos := start + 2 + uint64(i)*12
			if pos+12 > uint64(len(data)) {
				break
			}
			entry := data[pos : pos+12]
			if order.Uint16(entry[:2]) == 0x0112 && order.Uint16(entry[2:4]) == 3 && order.Uint32(entry[4:8]) == 1 {
				orientation := order.Uint16(entry[8:10])
				return orientation >= 5 && orientation <= 8
			}
		}
	}
	return false
}
