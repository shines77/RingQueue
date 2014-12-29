
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>

#include "vs_stdint.h"

#include "mq.h"
#include "port.h"

#define QSZ     (1 << 10)
#define QMSK    (QSZ - 1)

struct queue {
	volatile uint32_t head;
	volatile uint32_t tail;
	void * q[QSZ];
};

struct queue *
queue_create() {
	struct queue * q = (struct queue *)malloc(sizeof(*q));
	memset(q, 0, sizeof(*q));
	return q;
}

void *
queue_push(struct queue *q, void *m) {
	uint32_t head, tail;
	do {
		tail = q->tail;
// maybe we don't need memory fence in x86
//		__sync_synchronize();
		head = q->head;
		if (head + QSZ == tail) {
			return NULL;
		}
	} while (!jimi_bool_compare_and_swap32(&q->tail, tail, tail+1));
	q->q[tail & QMSK] = m;

	return m;
}

void *
queue_pop(struct queue *q) {
	uint32_t head, tail, masked;
	void * m;
	do {
		head = q->head;
		tail = q->tail;
		if (head == tail) {
			return NULL;
		}
		masked = head & QMSK;
		m = q->q[masked];
		if (m == NULL) {
			return NULL;
		}
#if defined(__clang__) || defined(__CLANG__) || defined(__APPLE__) || defined(__FreeBSD__)
    } while (0);
#else
  #if defined(_M_X64) || defined(_WIN64)
	} while (!jimi_bool_compare_and_swap64(&q->q[masked], m, NULL));
  #else
    } while (!jimi_bool_compare_and_swap32(&q->q[masked], m, NULL));
  #endif
#endif
	q->head ++;
	return m;
}
