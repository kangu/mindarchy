#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int mindarchy_automerge_roundtrip(void);
int mindarchy_automerge_write_fixture(const char *path);
int mindarchy_automerge_read_fixture(const char *path);

#ifdef __cplusplus
}
#endif
