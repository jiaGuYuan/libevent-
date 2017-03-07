/*
 * Copyright (c) 2007-2012 Niels Provos and Nick Mathewson
 *
 * Copyright (c) 2006 Maxim Yegorushkin <maxim.yegorushkin@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef MINHEAP_INTERNAL_H_INCLUDED_
#define MINHEAP_INTERNAL_H_INCLUDED_

#include "event2/event-config.h"
#include "evconfig-private.h"
#include "event2/event.h"
#include "event2/event_struct.h"
#include "event2/util.h"
#include "util-internal.h"
#include "mm-internal.h"


/* 最小堆: 是一个完全二叉树,父节点的值总是小于等于子节点的值.(堆一般是用数组来存储的) */
typedef struct min_heap
{
	struct event** p; //指向存储最小堆元素的空间
	unsigned n, a; //n:队列元素的个数; a:队列空间的大小 
} min_heap_t;

static inline void	     min_heap_ctor_(min_heap_t* s);
static inline void	     min_heap_dtor_(min_heap_t* s);
static inline void	     min_heap_elem_init_(struct event* e);
static inline int	     min_heap_elt_is_top_(const struct event *e);
static inline int	     min_heap_empty_(min_heap_t* s);
static inline unsigned	     min_heap_size_(min_heap_t* s);
static inline struct event*  min_heap_top_(min_heap_t* s);
static inline int	     min_heap_reserve_(min_heap_t* s, unsigned n);
static inline int	     min_heap_push_(min_heap_t* s, struct event* e);
static inline struct event*  min_heap_pop_(min_heap_t* s);
static inline int	     min_heap_adjust_(min_heap_t *s, struct event* e);
static inline int	     min_heap_erase_(min_heap_t* s, struct event* e);
static inline void	     min_heap_shift_up_(min_heap_t* s, unsigned hole_index, struct event* e);
static inline void	     min_heap_shift_up_unconditional_(min_heap_t* s, unsigned hole_index, struct event* e);
static inline void	     min_heap_shift_down_(min_heap_t* s, unsigned hole_index, struct event* e);

#define min_heap_elem_greater(a, b) \
	(evutil_timercmp(&(a)->ev_timeout, &(b)->ev_timeout, >))

//初始化最小堆
void min_heap_ctor_(min_heap_t* s) { s->p = 0; s->n = 0; s->a = 0; }
//注销最小堆
void min_heap_dtor_(min_heap_t* s) { if (s->p) mm_free(s->p); }
//初始化最小堆的元素
void min_heap_elem_init_(struct event* e) { e->ev_timeout_pos.min_heap_idx = -1; }
//测试最小堆是否为空
int min_heap_empty_(min_heap_t* s) { return 0u == s->n; }
//返回最小堆的元素个数
unsigned min_heap_size_(min_heap_t* s) { return s->n; }
//返回最小堆的根元素。 没有元素时返回0
struct event* min_heap_top_(min_heap_t* s) { return s->n ? *s->p : 0; }

//将e插入最小堆
int min_heap_push_(min_heap_t* s, struct event* e)
{
	if (min_heap_reserve_(s, s->n + 1))
		return -1;
	min_heap_shift_up_(s, s->n++, e);
	return 0;
}

//从最小堆中删除根(删除节点的空间并没有释放，需要手动释放返回指针的空间)
struct event* min_heap_pop_(min_heap_t* s)
{
	if (s->n)
	{
		struct event* e = *s->p;
		min_heap_shift_down_(s, 0u, s->p[--s->n]);
		e->ev_timeout_pos.min_heap_idx = -1;
		return e;
	}
	return 0;
}

//测试e是否为最小堆的根
int min_heap_elt_is_top_(const struct event *e)
{
	return e->ev_timeout_pos.min_heap_idx == 0; //根元素的min_heap_idx为0
}

//删除最小堆中的e指向的元素
int min_heap_erase_(min_heap_t* s, struct event* e)
{
	if (-1 != e->ev_timeout_pos.min_heap_idx)
	{
		struct event *last = s->p[--s->n];//把最后一个值作为要填入hole_index的值(同时s->n的值减一:因为删除了一个元素)
		unsigned parent = (e->ev_timeout_pos.min_heap_idx - 1) / 2;
		/* we replace e with the last element in the heap.  We might need to
		   shift it upward if it is less than its parent, or downward if it is
		   greater than one or both its children. Since the children are known
		   to be less than the parent, it can't need to shift both up and
		   down. */
		//用最后一个元素去填补删除e留下的空缺(这里不是简单的替换,因为填补空缺之后依然需要满足最小堆)
		if (e->ev_timeout_pos.min_heap_idx > 0 && min_heap_elem_greater(s->p[parent], last))
			min_heap_shift_up_unconditional_(s, e->ev_timeout_pos.min_heap_idx, last);
		else
			min_heap_shift_down_(s, e->ev_timeout_pos.min_heap_idx, last);
		e->ev_timeout_pos.min_heap_idx = -1;//标记删除
		return 0;
	}
	return -1;
}

//将e调整到最小堆的合适位置(e可以是新元素，也可以是最小堆中的元素)
int min_heap_adjust_(min_heap_t *s, struct event *e)
{
	if (-1 == e->ev_timeout_pos.min_heap_idx) {
		return min_heap_push_(s, e);
	} else {
		unsigned parent = (e->ev_timeout_pos.min_heap_idx - 1) / 2;
		/* The position of e has changed; we shift it up or down
		 * as needed.  We can't need to do both. */
		if (e->ev_timeout_pos.min_heap_idx > 0 && min_heap_elem_greater(s->p[parent], e))
			min_heap_shift_up_unconditional_(s, e->ev_timeout_pos.min_heap_idx, e);
		else
			min_heap_shift_down_(s, e->ev_timeout_pos.min_heap_idx, e);
		return 0;
	}
}

//分配队列大小.n代表队列元素个数多少
int min_heap_reserve_(min_heap_t* s, unsigned n)
{
	if (s->a < n)//队列大小不足元素个数,重新分配空间.
	{
		struct event** p;
		unsigned a = s->a ? s->a * 2 : 8; //初始分配8个指针大小空间,否则原空间大小翻倍.
		if (a < n)
			a = n; //翻倍后空间依旧不足,则分配n
		if (!(p = (struct event**)mm_realloc(s->p, a * sizeof *p)))//重新分配内存
			return -1;
		s->p = p; //重新赋值队列地址及大小.
		s->a = a;
	}
	return 0;
}

/*功能:调整最小堆、用e替换掉索引为hole_index的节点,并将e向hole_index的父节点进行至少一次调整;
  如果不满足最小堆的条件 则顺着最小堆从叶子节点到根的方向调整元素位置
  hole_index:要替换的节点索引  e:新的元素 
  这个函数至少会进行一次调整。调用这个函数需要保证e的值小于hole_index的父节点的值
  */
void min_heap_shift_up_unconditional_(min_heap_t* s, unsigned hole_index, struct event* e)
{
    unsigned parent = (hole_index - 1) / 2; //计算hole_index节点的父节点的索引值 
    do
    {
		(s->p[hole_index] = s->p[parent])->ev_timeout_pos.min_heap_idx = hole_index;
		hole_index = parent;
		parent = (hole_index - 1) / 2;
    } while (hole_index && min_heap_elem_greater(s->p[parent], e));
    (s->p[hole_index] = e)->ev_timeout_pos.min_heap_idx = hole_index;
}

/*功能:调整最小堆、用e替换掉索引为hole_index的节点。如果替换之后不满足最小堆的条件则向着最小堆根方向调整元素位置
  hole_index:要替换的节点索引  e:新的元素 
  这个函数的作用是:当hloe_index位置替换e后不满足最小堆的条件,则将e沿着叶子到根的方向调整到合适的位置。
  		当hole_index为s->n时相当于插入元素到最小堆中
*/
void min_heap_shift_up_(min_heap_t* s, unsigned hole_index, struct event* e)
{
    unsigned parent = (hole_index - 1) / 2; //计算hole_index节点的父节点的索引值 
	//比父节点小或是到达根节点.则交换位置.循环.
	while (hole_index && min_heap_elem_greater(s->p[parent], e))
    {
		(s->p[hole_index] = s->p[parent])->ev_timeout_pos.min_heap_idx = hole_index;
		hole_index = parent;
		parent = (hole_index - 1) / 2;
    }
    (s->p[hole_index] = e)->ev_timeout_pos.min_heap_idx = hole_index;
}

/*功能:调整最小堆、用e替换掉索引为hole_index的节点。如果替换之后不满足最小堆的条件
       则顺着最小堆从根到叶子节点的方向调整元素位置
  hole_index:要替换的节点索引  e:新的元素 
  这个函数的作用是:当hloe_index位置替换e后不满足最小堆的条件,则将e沿着根到叶子节点的方向调整到合适的位置
*/
void min_heap_shift_down_(min_heap_t* s, unsigned hole_index, struct event* e)
{
    unsigned min_child = 2 * (hole_index + 1);
    while (min_child <= s->n)
	{	
		//找出较小子节点(判断是左孩子小还是右孩子小,还决定要不要减1)
		min_child -= min_child == s->n || min_heap_elem_greater(s->p[min_child], s->p[min_child - 1]);
		//比子节点小.不需要再交换位置,跳出循环.
		if (!(min_heap_elem_greater(e, s->p[min_child])))
		    break;
		//比子节点大,要交换位置
		(s->p[hole_index] = s->p[min_child])->ev_timeout_pos.min_heap_idx = hole_index;
		hole_index = min_child;
		min_child = 2 * (hole_index + 1);
	}
    (s->p[hole_index] = e)->ev_timeout_pos.min_heap_idx = hole_index;
}

#endif /* MINHEAP_INTERNAL_H_INCLUDED_ */
