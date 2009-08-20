/* (C)opyright 2009 Anton Novikov
 * See LICENSE file for license details.
 */

#ifndef UTIL_H
#define UTIL_H

struct pimpl_ptr_base
{
	typedef void (*deleter_t)(void *);

	deleter_t deleter;
	void *pimpl;

	pimpl_ptr_base(deleter_t d, void *p) : deleter(d), pimpl(p) { }
	~pimpl_ptr_base() { deleter(pimpl); }

private:
	pimpl_ptr_base(const pimpl_ptr_base &);
	pimpl_ptr_base &operator =(const pimpl_ptr_base &);
};

template <class T>
class pimpl_ptr : private pimpl_ptr_base
{
	static void do_delete(void *p) { delete static_cast<T *>(p); }

public:
	pimpl_ptr() : pimpl_ptr_base(do_delete, new T()) { }

	template <class A>
	explicit pimpl_ptr(A a) : pimpl_ptr_base(do_delete, new T(a)) { }

	T *get()                     { return static_cast<T *>(pimpl); }
	const T *get() const         { return static_cast<const T *>(pimpl); }

	T *operator ->()             { return get(); }
	const T *operator ->() const { return get(); }

	operator T *()               { return get(); }
	operator T *() const         { return get(); }
};

#endif /* UTIL_H */
