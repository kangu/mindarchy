#include "collaboration_compat_shim.h"

#include <automerge.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int save_document(const AMdoc *doc, const char *path) {
    AMresult *saved = AMsave((AMdoc *)doc);
    if (!saved || !AMresultItem(saved)) return 1;
    AMbyteSpan bytes = {0};
    if (!AMitemToBytes(AMresultItem(saved), &bytes) || !bytes.src || !bytes.count) return 2;
    FILE *file = fopen(path, "wb");
    if (!file) return 3;
    const size_t written = fwrite(bytes.src, 1, bytes.count, file);
    fclose(file);
    AMresultFree(saved);
    return written == bytes.count ? 0 : 4;
}

int mindarchy_automerge_roundtrip(void) {
    AMresult *created = AMcreate(NULL);
    if (!created || !AMresultItem(created)) return 1;

    const AMdoc *created_doc = NULL;
    if (!AMitemToDoc(AMresultItem(created), &created_doc) || !created_doc) return 2;

    AMresult *saved = AMsave((AMdoc *)created_doc);
    if (!saved || !AMresultItem(saved)) return 3;
    AMbyteSpan saved_bytes = {0};
    if (!AMitemToBytes(AMresultItem(saved), &saved_bytes) || !saved_bytes.src || !saved_bytes.count) return 4;

    uint8_t *copy = malloc(saved_bytes.count);
    if (!copy) return 5;
    for (size_t i = 0; i < saved_bytes.count; ++i) copy[i] = saved_bytes.src[i];
    const size_t copy_size = saved_bytes.count;
    AMresultFree(saved);

    AMresult *loaded = AMload(copy, copy_size);
    free(copy);
    if (!loaded || !AMresultItem(loaded)) return 6;
    const AMdoc *loaded_doc = NULL;
    if (!AMitemToDoc(AMresultItem(loaded), &loaded_doc) || !loaded_doc) return 7;

    AMresultFree(loaded);
    AMresultFree(created);
    return 0;
}

int mindarchy_automerge_write_fixture(const char *path) {
    AMresult *created = AMcreate(NULL);
    if (!created || !AMresultItem(created)) return 1;
    const AMdoc *doc = NULL;
    if (!AMitemToDoc(AMresultItem(created), &doc) || !doc) return 2;
    AMresult *put = AMmapPutStr((AMdoc *)doc, AM_ROOT, AMstr("title"), AMstr("cpp-created"));
    if (!put) return 3;
    AMresultFree(put);
    AMresult *commit = AMcommit((AMdoc *)doc, AMstr("fixture"), NULL);
    if (!commit) return 4;
    AMresultFree(commit);
    const int result = save_document(doc, path);
    AMresultFree(created);
    return result;
}

int mindarchy_automerge_read_fixture(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return 1;
    if (fseek(file, 0, SEEK_END) != 0) return 2;
    const long length = ftell(file);
    if (length <= 0 || fseek(file, 0, SEEK_SET) != 0) return 3;
    uint8_t *bytes = malloc((size_t)length);
    if (!bytes) return 4;
    const size_t read = fread(bytes, 1, (size_t)length, file);
    fclose(file);
    if (read != (size_t)length) { free(bytes); return 5; }
    AMresult *loaded = AMload(bytes, read);
    free(bytes);
    if (!loaded || !AMresultItem(loaded)) return 6;
    const AMdoc *doc = NULL;
    if (!AMitemToDoc(AMresultItem(loaded), &doc) || !doc) return 7;
    AMresult *value = AMmapGet(doc, AM_ROOT, AMstr("title"), NULL);
    AMbyteSpan title = {0};
    const int result = value && AMresultItem(value) && AMitemToStr(AMresultItem(value), &title) &&
        title.count == strlen("go-created") && memcmp(title.src, "go-created", title.count) == 0 ? 0 : 8;
    if (value) AMresultFree(value);
    AMresultFree(loaded);
    return result;
}
