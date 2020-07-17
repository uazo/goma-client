// Copyright 2020 Google LLC. All Rights Reserved.

// Binary generate_feature_lists generates the list of clangs features.
//
// See: http://clang.llvm.org/docs/LanguageExtensions.html#feature-checking-macros
//
// Usage:
//   $ go run generate_feature_lists.go -r $commit -o clang_features.cc
package main

import (
	"bufio"
	"bytes"
	"context"
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"os/exec"
	"path"
	"regexp"
	"sort"
	"strings"
	"text/template"
	"time"
)

var (
	revision     string
	listRevision = flag.Bool("list_revision", false, "list revisions")
	output       = flag.String("o", "clang_features.cc", "output filename")
	verbose      = flag.Bool("v", false, "verbose")
)

func init() {
	flag.StringVar(&revision, "revision", "HEAD",
		"LLVM revision (commit) to read in order to generate the features.")
	flag.StringVar(&revision, "r", "HEAD",
		"alias of --revision")
}

const (
	llvmRepo = "https://github.com/llvm/llvm-project"
	llvmTop  = "https://raw.githubusercontent.com/llvm/llvm-project/"
)

var commitHashRE = regexp.MustCompile("[^0-9a-f]{40}")

func latestRevision(ctx context.Context, revision string) (string, error) {
	if commitHashRE.MatchString(revision) {
		return revision, nil
	}
	cmd := exec.Command("git", "ls-remote", llvmRepo)
	output, err := cmd.Output()
	if err != nil {
		return "", fmt.Errorf("git ls-remote %s failed: %v\n%s", llvmRepo, err, output)
	}
	if *listRevision {
		fmt.Println(string(output))
		os.Exit(0)
	}
	ref := revision
	revision = ""
	s := bufio.NewScanner(bytes.NewReader(output))
	for s.Scan() {
		line := s.Text()
		if strings.HasSuffix(line, ref) {
			revision = strings.TrimSpace(strings.TrimSuffix(line, ref))
			break
		}
	}
	err = s.Err()
	if err != nil {
		return "", err
	}
	if revision == "" {
		return "", fmt.Errorf("%s not found in %s", ref, llvmRepo)
	}
	return revision, nil
}

func mustFetch(ctx context.Context, revision, pathname string) string {
	reqURL := llvmTop + path.Join(revision, pathname)
	fmt.Println(reqURL)
	resp, err := http.Get(reqURL)
	if err != nil {
		log.Fatal(err)
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		log.Fatalf("get %s: %d %s", reqURL, resp.StatusCode, resp.Status)
	}
	body, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		log.Fatal(err)
	}
	return string(body)
}

func mustParse(ctx context.Context, body string, pattern string) [][]string {
	re := regexp.MustCompile(pattern)
	matches := re.FindAllStringSubmatch(body, -1)
	return matches
}

func captures(matches [][]string, n int) []string {
	var ret []string
	for _, m := range matches {
		if len(m) <= n {
			log.Fatalf("match %q: want %d groups", m, n)
		}
		ret = append(ret, m[n])
	}
	return ret
}

var tmpl = template.Must(template.New("").Parse(`// Copyright {{.Year}} Google LLC. All Rights Reserved.
//
// This is auto-generated file by
//   go run generate_feature_lists.go {{.Args}}.
//
// LLVM revison: {{.Revision}}
// *** DO NOT EDIT ***

#include "absl/base/macros.h"

static const char* KNOWN_FEATURES[] = {
{{- range .Features}}
    {{printf "%q" .}},
{{- end}}
};

static const unsigned long NUM_KNOWN_FEATURES =
  ABSL_ARRAYSIZE(KNOWN_FEATURES);

static const char* KNOWN_EXTENSIONS[] = {
{{- range .Extensions}}
    {{printf "%q" .}},
{{- end}}
};

static const unsigned long NUM_KNOWN_EXTENSIONS =
  ABSL_ARRAYSIZE(KNOWN_EXTENSIONS);

static const char* KNOWN_ATTRIBUTES[] = {
{{- range .Attrs}}
    {{printf "%q" .}},
{{- end}}
};

static const unsigned long NUM_KNOWN_ATTRIBUTES =
  ABSL_ARRAYSIZE(KNOWN_ATTRIBUTES);

static const char* KNOWN_CPP_ATTRIBUTES[] = {
{{- range .CPPAttrs}}
    {{printf "%q" .}},
{{- end}}
};

static const unsigned long NUM_KNOWN_CPP_ATTRIBUTES =
 ABSL_ARRAYSIZE(KNOWN_CPP_ATTRIBUTES);

static const char* KNOWN_DECLSPEC_ATTRIBUTES[] = {
{{- range .DeclSpecs}}
    {{printf "%q" .}},
{{- end}}
};

static const unsigned long NUM_KNOWN_DECLSPEC_ATTRIBUTES =
  ABSL_ARRAYSIZE(KNOWN_DECLSPEC_ATTRIBUTES);

static const char* KNOWN_BUILTINS[] = {
{{- range .Builtins}}
    {{printf "%q" .}},
{{- end}}
};

static const unsigned long NUM_KNOWN_BUILTINS =
  ABSL_ARRAYSIZE(KNOWN_BUILTINS);

static const char* KNOWN_WARNINGS[] = {
{{- range .Diagnostics}}
    {{printf "%q" .}},
{{- end}}
};

static const unsigned long NUM_KNOWN_WARNINGS =
  ABSL_ARRAYSIZE(KNOWN_WARNINGS);
`))

func main() {
	flag.Parse()
	ctx := context.Background()

	revision, err := latestRevision(ctx, revision)
	if err != nil {
		log.Fatal(err)
	}
	fmt.Println(revision)

	featuresDef := mustFetch(ctx, revision, "clang/include/clang/Basic/Features.def")

	features := captures(mustParse(ctx, featuresDef, `(?ms)^FEATURE\((\w+),`), 1)
	sort.Strings(features)
	extensions := captures(mustParse(ctx, featuresDef, `(?ms)^EXTENSION\((\w+),`), 1)
	sort.Strings(extensions)

	attrTD := mustFetch(ctx, revision, "clang/include/clang/Basic/Attr.td")
	seenAttr := map[string]bool{}
	var attrs []string
	seenCPPAttrs := map[string]bool{}
	var cppAttrs []string
	seenDeclSpecs := map[string]bool{}
	var declSpecs []string

	for _, spelling := range captures(mustParse(ctx, attrTD, `(?ms)let Spellings\s+=\s+\[(.*?)\];`), 1) {
		if *verbose {
			log.Printf("spelling: %q", spelling)
		}
		if spelling == "" {
			continue
		}
		for _, entry := range mustParse(ctx, spelling, `(?ms)([<]*)<"([^>]*)"(:?,[^"]*)?>`) {
			if string(entry[1]) == "Pragma" {
				// Ignore attribute used with pragma.
				// Pragma seems to be only used for #pragma.
				// It also caused the issue. (b/63365915)
				continue
			}
			attr := string(entry[2])
			if i := strings.IndexByte(attr, '"'); i >= 0 {
				l := strings.Split(attr, `"`)
				attr = l[len(l)-1]
			}
			if strings.Contains(attr, " ") {
				continue
			}
			if seenAttr[attr] {
				continue
			}
			attrs = append(attrs, attr)
			seenAttr[attr] = true
		}
		for _, entry := range mustParse(ctx, spelling, `(?ms)CXX11<"([^"]*)",\s*"([^"]*)"(.*?)>`) {
			namespace := string(entry[1])
			name := string(entry[2])
			if namespace != "" {
				name = namespace + "::" + name
			}
			if seenCPPAttrs[name] {
				continue
			}
			cppAttrs = append(cppAttrs, name)
			seenCPPAttrs[name] = true
		}
		for _, entry := range mustParse(ctx, spelling, `Declspec<"(.*?)">`) {
			name := string(entry[1])
			if seenDeclSpecs[name] {
				continue
			}
			declSpecs = append(declSpecs, name)
			seenDeclSpecs[name] = true
		}
	}
	sort.Strings(attrs)
	sort.Strings(cppAttrs)
	sort.Strings(declSpecs)

	builtinsDef := mustFetch(ctx, revision, "clang/include/clang/Basic/Builtins.def")
	builtins := captures(mustParse(ctx, builtinsDef, `(?ms)^[A-Z_]*BUILTIN\((\w+),`), 1)
	sort.Strings(builtins)

	// TODO: There may be a better way to obtain all the diagnostic flags.
	// This reference mentions that it is generated by `clang-tblgen -gen-diag-docs`.
	// Unfortunately I couldn't find the input to `clang-tblgen`.
	diagnosticsDef := mustFetch(ctx, revision, "clang/docs/DiagnosticsReference.rst")
	// Example diagonistic (warning) flags:
	// -Wall
	// -W#warnings
	// -Watomic-alighment
	// -Wc++11-compat
	// -Wstrict-overflow=1
	// ...
	// Note: I couldn't spot a single diagnostic flag containing '_'.
	diagnostics := captures(mustParse(ctx, diagnosticsDef, `(?ms)(-W[a-zA-Z0-9+#=\-]+)\n`), 1)
	sort.Strings(diagnostics)

	data := struct {
		Year        string
		Args        string
		Revision    string
		Features    []string
		Extensions  []string
		Attrs       []string
		CPPAttrs    []string
		DeclSpecs   []string
		Builtins    []string
		Diagnostics []string
	}{
		Year:        time.Now().Format("2006"),
		Args:        strings.Join(os.Args[1:], " "),
		Revision:    revision,
		Features:    features,
		Extensions:  extensions,
		Attrs:       attrs,
		CPPAttrs:    cppAttrs,
		DeclSpecs:   declSpecs,
		Builtins:    builtins,
		Diagnostics: diagnostics,
	}
	var buf bytes.Buffer
	err = tmpl.Execute(&buf, data)
	if err != nil {
		log.Fatal(err)
	}
	cmd := exec.Command("clang-format")
	cmd.Stdin = bytes.NewReader(buf.Bytes())
	code, err := cmd.Output()
	if err != nil {
		log.Fatalf("clang-format error: %v\n%s", err, output)
	}
	if *output == "" || *output == "-" {
		fmt.Println(string(code))
		return
	}
	err = ioutil.WriteFile(*output, code, 0644)
	if err != nil {
		log.Fatal(err)
	}
}
