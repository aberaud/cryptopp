#ifndef CRYPTOPP_SMARTPTR_H
#define CRYPTOPP_SMARTPTR_H

#include "config.h"
#include "stdcpp.h"
#include "trap.h"

NAMESPACE_BEGIN(CryptoPP)

// CryptoPP::auto_ptr in created in both cases
#if defined(CRYPTOPP_CXX11_UNIQUE_PTR) && defined(CRYPTOPP_CXX11_TEMPLATE_ALIAS)
  template<typename T>
    using auto_ptr = std::unique_ptr<T>;
#else
  using std::auto_ptr;
#endif

template <class T> class simple_ptr
{
public:
	simple_ptr(T *p = NULL) : m_p(p) {}
	~simple_ptr();

public:
	T *m_p;

private:
	simple_ptr(const simple_ptr<T>& rhs);		// copy not allowed
	void operator=(const simple_ptr<T>& rhs);	// assignment not allowed
};

// Set m_p to NULL so double destruction (which might occur in Singleton) will be harmless
template <class T> simple_ptr<T>::~simple_ptr()
{
	delete m_p;
	*((volatile T**)(&m_p)) = 0;

	// Ensure the assignment is always performed. MSVC and Clang provide expected
	//   operational behavior for volatile. GCC has a more strict interpretation of
	//   the keyword (see http://www.airs.com/blog/archives/154), and volatile
	//   should not be used to tame the optimizer. However, inline assembly
	//   will tame it (see https://gcc.gnu.org/ml/gcc-help/2015-07/msg00053.html).
#ifdef __GNUC__
	asm volatile ("" : : : "memory");
#endif
}

template <class T> class member_ptr
{
public:
	explicit member_ptr(T *p = NULL) : m_p(p) {}

	~member_ptr();

	const T& operator*() const { return *m_p; }
	T& operator*() { return *m_p; }

	const T* operator->() const { return m_p; }
	T* operator->() { return m_p; }

	const T* get() const { return m_p; }
	T* get() { return m_p; }

	T* release()
	{
		T *old_p = m_p;
		*((volatile T**)(&m_p)) = 0;
		return old_p;

#ifdef __GNUC__
	asm volatile ("" : : : "memory");
#endif
	}

	void reset(T *p = 0);

protected:
	T *m_p;

private:
	member_ptr(const member_ptr<T>& rhs);		// copy not allowed
	void operator=(const member_ptr<T>& rhs);	// assignment not allowed
};

template <class T> member_ptr<T>::~member_ptr() {delete m_p;}
template <class T> void member_ptr<T>::reset(T *p) {delete m_p; m_p = p;}

// ********************************************************

template<class T> class value_ptr : public member_ptr<T>
{
public:
	value_ptr(const T &obj) : member_ptr<T>(new T(obj)) {}
	value_ptr(T *p = NULL) : member_ptr<T>(p) {}
	value_ptr(const value_ptr<T>& rhs)
		: member_ptr<T>(rhs.m_p ? new T(*rhs.m_p) : NULL) {}

	value_ptr<T>& operator=(const value_ptr<T>& rhs);
	bool operator==(const value_ptr<T>& rhs)
	{
		return (!this->m_p && !rhs.m_p) || (this->m_p && rhs.m_p && *this->m_p == *rhs.m_p);
	}
};

template <class T> value_ptr<T>& value_ptr<T>::operator=(const value_ptr<T>& rhs)
{
	if (this != &rhs)
	{
		T *old_p = this->m_p;
		this->m_p = rhs.m_p ? new T(*rhs.m_p) : NULL;
		delete old_p;
	}
	return *this;
}

// ********************************************************

template<class T> class clonable_ptr : public member_ptr<T>
{
public:
	clonable_ptr(const T &obj) : member_ptr<T>(obj.Clone()) {}
	clonable_ptr(T *p = NULL) : member_ptr<T>(p) {}
	clonable_ptr(const clonable_ptr<T>& rhs)
		: member_ptr<T>(rhs.m_p ? rhs.m_p->Clone() : NULL) {}

	clonable_ptr<T>& operator=(const clonable_ptr<T>& rhs);
};

template <class T> clonable_ptr<T>& clonable_ptr<T>::operator=(const clonable_ptr<T>& rhs)
{
	if (this !=  &rhs)
	{
		T *old_p = this->m_p;
		this->m_p = rhs.m_p ? rhs.m_p->Clone() : NULL;
		delete old_p;
	}
	return *this;
}

// ********************************************************

template<class T> class counted_ptr
{
public:
	explicit counted_ptr(T *p = 0);
	counted_ptr(const T &r) : m_p(0) {attach(r);}
	counted_ptr(const counted_ptr<T>& rhs);

	~counted_ptr();

	const T& operator*() const { return *m_p; }
	T& operator*() { return *m_p; }

	const T* operator->() const { return m_p; }
	T* operator->() { return get(); }

	const T* get() const { return m_p; }
	T* get();

	void attach(const T &p);

	counted_ptr<T> & operator=(const counted_ptr<T>& rhs);

protected:
	T *m_p;
};

template <class T> counted_ptr<T>::counted_ptr(T *p)
	: m_p(p)
{
	if (m_p)
		m_p->m_referenceCount = 1;
}

template <class T> counted_ptr<T>::counted_ptr(const counted_ptr<T>& rhs)
	: m_p(rhs.m_p)
{
	if (m_p)
		m_p->m_referenceCount++;
}

template <class T> counted_ptr<T>::~counted_ptr()
{
	if (m_p && --m_p->m_referenceCount == 0)
		delete m_p;
}

template <class T> void counted_ptr<T>::attach(const T &r)
{
	if (m_p && --m_p->m_referenceCount == 0)
		delete m_p;
	if (r.m_referenceCount == 0)
	{
		m_p = r.clone();
		m_p->m_referenceCount = 1;
	}
	else
	{
		m_p = const_cast<T *>(&r);
		m_p->m_referenceCount++;
	}
}

template <class T> T* counted_ptr<T>::get()
{
	if (m_p && m_p->m_referenceCount > 1)
	{
		T *temp = m_p->clone();
		m_p->m_referenceCount--;
		m_p = temp;
		m_p->m_referenceCount = 1;
	}
	return m_p;
}

template <class T> counted_ptr<T> & counted_ptr<T>::operator=(const counted_ptr<T>& rhs)
{
	if (this == &rhs) { return *this; }

	if (m_p != rhs.m_p)
	{
		if (m_p && --m_p->m_referenceCount == 0)
			delete m_p;
		m_p = rhs.m_p;
		if (m_p)
			m_p->m_referenceCount++;
	}
	return *this;
}

// ********************************************************

template <class T> class vector_member_ptrs
{
public:
	vector_member_ptrs(size_t size=0)
		: m_size(size), m_ptr(new member_ptr<T>[size]) {}
	~vector_member_ptrs()
		{delete [] this->m_ptr;}

	member_ptr<T>& operator[](size_t index)
		{CRYPTOPP_ASSERT(index<this->m_size); return this->m_ptr[index];}
	const member_ptr<T>& operator[](size_t index) const
		{CRYPTOPP_ASSERT(index<this->m_size); return this->m_ptr[index];}

	size_t size() const {return this->m_size;}
	void resize(size_t newSize)
	{
		member_ptr<T> *newPtr = new member_ptr<T>[newSize];
		for (size_t i=0; i<this->m_size && i<newSize; i++)
			newPtr[i].reset(this->m_ptr[i].release());
		delete [] this->m_ptr;
		this->m_size = newSize;
		this->m_ptr = newPtr;
	}

private:
	vector_member_ptrs(const vector_member_ptrs<T> &c);	// copy not allowed
	void operator=(const vector_member_ptrs<T> &x);		// assignment not allowed

	size_t m_size;
	member_ptr<T> *m_ptr;
};

NAMESPACE_END

#endif
