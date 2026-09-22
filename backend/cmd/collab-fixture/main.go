package main

import (
	"flag"
	"fmt"
	"os"

	"github.com/automerge/automerge-go"
)

func main() {
	path := flag.String("path", "", "fixture path")
	expect := flag.String("expect", "", "expected title when loading")
	flag.Parse()
	if *path == "" {
		fmt.Fprintln(os.Stderr, "-path is required")
		os.Exit(2)
	}
	if *expect != "" {
		data, err := os.ReadFile(*path)
		if err != nil { panic(err) }
		doc, err := automerge.Load(data)
		if err != nil { panic(err) }
		value, err := doc.RootMap().Get("title")
		if err != nil || value.Str() != *expect {
			panic(fmt.Sprintf("title = %q, want %q", value.Str(), *expect))
		}
		return
	}
	doc := automerge.New()
	if err := doc.RootMap().Set("title", "go-created"); err != nil { panic(err) }
	if _, err := doc.Commit("fixture"); err != nil { panic(err) }
	if err := os.WriteFile(*path, doc.Save(), 0600); err != nil { panic(err) }
}
