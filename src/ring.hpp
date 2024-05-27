//// HEADER-ONLY LIBRARY. To include impl define RING_IMPL

#ifndef RING_HPP
#define RING_HPP

#include <stddef.h>

#define PERHAPS_IMPL
#include "perhaps.hpp"

template<typename T>
struct Ring {
	Ring();
	~Ring();

	Perhaps<T> read(); // consume and return the next character
	Perhaps<T> peek() const ; // return the next character
	void write(T item);
	void write_many(T buf[], size_t len);
	size_t len() const { return m_len; }
	size_t cap() const { return m_cap; }

private:
	T* m_buf;
	size_t m_read;
	size_t m_write;
	size_t m_len;
	size_t m_cap;
};

#ifdef RING_IMPL

#include <assert.h>

// arbitrary choice
#define RING_DEFAULT_CAP 1024

template<typename T>
Ring<T>::Ring() {
	m_buf = new T[RING_DEFAULT_CAP];
	m_read = 0;
	m_write = 0;
	m_len = 0;
	m_cap = RING_DEFAULT_CAP;
}

template<typename T>
Ring<T>::~Ring() { delete []m_buf; }

template<typename T>
Perhaps<T> Ring<T>::read() {
	if (m_len == 0) return Perhaps<T>();
	size_t idx = m_read;
	assert(idx <= (m_cap - 1));
	m_read = (m_read + 1) % m_cap;
	m_len = (m_len > 0) ? m_len - 1 : 0;
	return Perhaps<T>(m_buf[idx]);
}

template<typename T>
Perhaps<T> Ring<T>::peek() const {
	if (m_len == 0) return Perhaps<T>();
	size_t idx = m_read;
	assert(idx <= (m_cap - 1));
	return Perhaps<T>(m_buf[idx]);
}

template<typename T>
void Ring<T>::write(T c) {
	size_t idx = m_write;
	assert(idx <= (m_cap - 1));
	m_write = (m_write + 1) % m_cap;
	m_len = (m_len < m_cap) ? m_len + 1 : m_cap;
	m_buf[idx] = c;
}

template<typename T>
void Ring<T>::write_many(T buf[], size_t len) {
	assert(len <= (m_cap - m_len));
	for (size_t i = 0; i < len; i++) write(buf[i]);
}

#endif

#endif
