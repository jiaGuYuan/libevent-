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


/* ��С��: ��һ����ȫ������,���ڵ��ֵ����С�ڵ����ӽڵ��ֵ.(��һ�������������洢��) */
typedef struct min_heap
{
	struct event** p; //ָ��洢��С��Ԫ�صĿռ�
	unsigned n, a; //n:����Ԫ�صĸ���; a:���пռ�Ĵ�С 
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

//��ʼ����С��
void min_heap_ctor_(min_heap_t* s) { s->p = 0; s->n = 0; s->a = 0; }
//ע����С��
void min_heap_dtor_(min_heap_t* s) { if (s->p) mm_free(s->p); }
//��ʼ����С�ѵ�Ԫ��
void min_heap_elem_init_(struct event* e) { e->ev_timeout_pos.min_heap_idx = -1; }
//������С���Ƿ�Ϊ��
int min_heap_empty_(min_heap_t* s) { return 0u == s->n; }
//������С�ѵ�Ԫ�ظ���
unsigned min_heap_size_(min_heap_t* s) { return s->n; }
//������С�ѵĸ�Ԫ�ء� û��Ԫ��ʱ����0
struct event* min_heap_top_(min_heap_t* s) { return s->n ? *s->p : 0; }

//��e������С��
int min_heap_push_(min_heap_t* s, struct event* e)
{
	if (min_heap_reserve_(s, s->n + 1))
		return -1;
	min_heap_shift_up_(s, s->n++, e);
	return 0;
}

//����С����ɾ����(ɾ���ڵ�Ŀռ䲢û���ͷţ���Ҫ�ֶ��ͷŷ���ָ��Ŀռ�)
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

//����e�Ƿ�Ϊ��С�ѵĸ�
int min_heap_elt_is_top_(const struct event *e)
{
	return e->ev_timeout_pos.min_heap_idx == 0; //��Ԫ�ص�min_heap_idxΪ0
}

//ɾ����С���е�eָ���Ԫ��
int min_heap_erase_(min_heap_t* s, struct event* e)
{
	if (-1 != e->ev_timeout_pos.min_heap_idx)
	{
		struct event *last = s->p[--s->n];//�����һ��ֵ��ΪҪ����hole_index��ֵ(ͬʱs->n��ֵ��һ:��Ϊɾ����һ��Ԫ��)
		unsigned parent = (e->ev_timeout_pos.min_heap_idx - 1) / 2;
		/* we replace e with the last element in the heap.  We might need to
		   shift it upward if it is less than its parent, or downward if it is
		   greater than one or both its children. Since the children are known
		   to be less than the parent, it can't need to shift both up and
		   down. */
		//�����һ��Ԫ��ȥ�ɾ��e���µĿ�ȱ(���ﲻ�Ǽ򵥵��滻,��Ϊ���ȱ֮����Ȼ��Ҫ������С��)
		if (e->ev_timeout_pos.min_heap_idx > 0 && min_heap_elem_greater(s->p[parent], last))
			min_heap_shift_up_unconditional_(s, e->ev_timeout_pos.min_heap_idx, last);
		else
			min_heap_shift_down_(s, e->ev_timeout_pos.min_heap_idx, last);
		e->ev_timeout_pos.min_heap_idx = -1;//���ɾ��
		return 0;
	}
	return -1;
}

//��e��������С�ѵĺ���λ��(e��������Ԫ�أ�Ҳ��������С���е�Ԫ��)
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

//������д�С.n�������Ԫ�ظ�������
int min_heap_reserve_(min_heap_t* s, unsigned n)
{
	if (s->a < n)//���д�С����Ԫ�ظ���,���·���ռ�.
	{
		struct event** p;
		unsigned a = s->a ? s->a * 2 : 8; //��ʼ����8��ָ���С�ռ�,����ԭ�ռ��С����.
		if (a < n)
			a = n; //������ռ����ɲ���,�����n
		if (!(p = (struct event**)mm_realloc(s->p, a * sizeof *p)))//���·����ڴ�
			return -1;
		s->p = p; //���¸�ֵ���е�ַ����С.
		s->a = a;
	}
	return 0;
}

/*����:������С�ѡ���e�滻������Ϊhole_index�Ľڵ�,����e��hole_index�ĸ��ڵ��������һ�ε���;
  �����������С�ѵ����� ��˳����С�Ѵ�Ҷ�ӽڵ㵽���ķ������Ԫ��λ��
  hole_index:Ҫ�滻�Ľڵ�����  e:�µ�Ԫ�� 
  ����������ٻ����һ�ε������������������Ҫ��֤e��ֵС��hole_index�ĸ��ڵ��ֵ
  */
void min_heap_shift_up_unconditional_(min_heap_t* s, unsigned hole_index, struct event* e)
{
    unsigned parent = (hole_index - 1) / 2; //����hole_index�ڵ�ĸ��ڵ������ֵ 
    do
    {
		(s->p[hole_index] = s->p[parent])->ev_timeout_pos.min_heap_idx = hole_index;
		hole_index = parent;
		parent = (hole_index - 1) / 2;
    } while (hole_index && min_heap_elem_greater(s->p[parent], e));
    (s->p[hole_index] = e)->ev_timeout_pos.min_heap_idx = hole_index;
}

/*����:������С�ѡ���e�滻������Ϊhole_index�Ľڵ㡣����滻֮��������С�ѵ�������������С�Ѹ��������Ԫ��λ��
  hole_index:Ҫ�滻�Ľڵ�����  e:�µ�Ԫ�� 
  ���������������:��hloe_indexλ���滻e��������С�ѵ�����,��e����Ҷ�ӵ����ķ�����������ʵ�λ�á�
  		��hole_indexΪs->nʱ�൱�ڲ���Ԫ�ص���С����
*/
void min_heap_shift_up_(min_heap_t* s, unsigned hole_index, struct event* e)
{
    unsigned parent = (hole_index - 1) / 2; //����hole_index�ڵ�ĸ��ڵ������ֵ 
	//�ȸ��ڵ�С���ǵ�����ڵ�.�򽻻�λ��.ѭ��.
	while (hole_index && min_heap_elem_greater(s->p[parent], e))
    {
		(s->p[hole_index] = s->p[parent])->ev_timeout_pos.min_heap_idx = hole_index;
		hole_index = parent;
		parent = (hole_index - 1) / 2;
    }
    (s->p[hole_index] = e)->ev_timeout_pos.min_heap_idx = hole_index;
}

/*����:������С�ѡ���e�滻������Ϊhole_index�Ľڵ㡣����滻֮��������С�ѵ�����
       ��˳����С�ѴӸ���Ҷ�ӽڵ�ķ������Ԫ��λ��
  hole_index:Ҫ�滻�Ľڵ�����  e:�µ�Ԫ�� 
  ���������������:��hloe_indexλ���滻e��������С�ѵ�����,��e���Ÿ���Ҷ�ӽڵ�ķ�����������ʵ�λ��
*/
void min_heap_shift_down_(min_heap_t* s, unsigned hole_index, struct event* e)
{
    unsigned min_child = 2 * (hole_index + 1);
    while (min_child <= s->n)
	{	
		//�ҳ���С�ӽڵ�(�ж�������С�����Һ���С,������Ҫ��Ҫ��1)
		min_child -= min_child == s->n || min_heap_elem_greater(s->p[min_child], s->p[min_child - 1]);
		//���ӽڵ�С.����Ҫ�ٽ���λ��,����ѭ��.
		if (!(min_heap_elem_greater(e, s->p[min_child])))
		    break;
		//���ӽڵ��,Ҫ����λ��
		(s->p[hole_index] = s->p[min_child])->ev_timeout_pos.min_heap_idx = hole_index;
		hole_index = min_child;
		min_child = 2 * (hole_index + 1);
	}
    (s->p[hole_index] = e)->ev_timeout_pos.min_heap_idx = hole_index;
}

#endif /* MINHEAP_INTERNAL_H_INCLUDED_ */
