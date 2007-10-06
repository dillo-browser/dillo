#ifndef __TIMEOUT_HH__
#define __TIMEOUT_HH__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef void (*TimeoutCb_t)(void *data);

void a_Timeout_add(float t, TimeoutCb_t cb, void *cbdata);
void a_Timeout_repeat(float t, TimeoutCb_t cb, void *cbdata);
void a_Timeout_remove();


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __TIMEOUT_HH__ */

