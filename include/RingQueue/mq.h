
#ifndef _CLOUDWU_MQ_H_
#define _CLOUDWU_MQ_H_

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct queue;

struct queue *queue_create();

void *queue_push(struct queue *q, void *m);

void *queue_pop(struct queue *q);

#ifdef __cplusplus
}
#endif

#endif  /* !_CLOUDWU_MQ_H_ */
