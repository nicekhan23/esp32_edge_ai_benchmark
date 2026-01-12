// data_collection.h
#ifndef DATA_COLLECTION_H
#define DATA_COLLECTION_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void data_collection_init(void);
void data_collection_start(const char *source, const char *label);
void data_collection_add_sample(float sample);
void data_collection_finish(void);
void data_collection_finish_binary(float *samples, int count);

#ifdef __cplusplus
}
#endif

#endif /* DATA_COLLECTION_H */