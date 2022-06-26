#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*misc_update_function_t)(unsigned char *minecraft);
void misc_run_on_update(misc_update_function_t function);
void misc_run_on_tick(misc_update_function_t function);

void Level_saveLevelData_injection(unsigned char *level);

#ifdef __cplusplus
}
#endif