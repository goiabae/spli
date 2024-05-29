//// HEADER-ONLY LIBRARY. To include impl define RING_IMPL

#ifndef RING_HPP
#define RING_HPP

#include <debug/assertions.h>
#include <stddef.h>

#include <cstring>

#define PERHAPS_IMPL
#include "perhaps.hpp"

template<typename T>
struct Ring {
	Ring();
	~Ring();
	Ring(const Ring& other);
	Ring(Ring&& other) noexcept;
	Ring<T>& operator=(const Ring<T>& other);
	Ring<T>& operator=(Ring<T>&& other) noexcept;

	Perhaps<T> read();       // consume and return the next character
	Perhaps<T> peek() const; // return the next character
	void write(T item);
	void write_many(T buf[], size_t len);
	bool is_empty() const { return m_len == 0; }
	bool is_full() const { return m_len == m_cap; }

 private:
	T* m_buf;
	size_t m_read {0};
	size_t m_write {0};
	size_t m_len {0};
	size_t m_cap;
};

#ifdef RING_IMPL

#	include <assert.h>

// arbitrary choice
constexpr int RING_DEFAULT_CAP = 1024;

template<typename T>
Ring<T>::Ring() : m_buf(new T[RING_DEFAULT_CAP]), m_cap(RING_DEFAULT_CAP) {}

template<typename T>
Ring<T>::~Ring() {
	delete[] m_buf;
}

template<typename T>
Ring<T>::Ring(const Ring& other)
: m_read(other.m_read),
	m_write(other.m_write),
	m_len(other.m_len),
	m_cap(other.m_cap),
	m_buf(new T[other.m_cap]) {
	std::memcpy(m_buf, other.m_buf, sizeof(T) * other.m_cap);
}

template<typename T>
Ring<T>::Ring(Ring&& other) noexcept
: m_read(other.m_read),
	m_write(other.m_write),
	m_len(other.m_len),
	m_cap(other.m_cap),
	m_buf(std::move(other.m_buf)) {}

template<typename T>
Ring<T>& Ring<T>::operator=(const Ring<T>& other) {
	if (&other == this) return *this;
	m_read = other.m_read;
	m_write = other.m_write;
	m_len = other.m_len;
	m_cap = other.m_cap;
	m_buf = new T[other.m_cap];
	std::memcpy(m_buf, other.m_buf, sizeof(T) * other.m_cap);
	return *this;
}

template<typename T>
Ring<T>& Ring<T>::operator=(Ring<T>&& other) noexcept {
	if (&other == this) return *this;
	m_read = other.m_read;
	m_write = other.m_write;
	m_len = other.m_len;
	m_cap = other.m_cap;
	m_buf = std::move(other.m_buf);
	return *this;
}

template<typename T>
Perhaps<T> Ring<T>::read() {
	if (m_len == 0) return Perhaps<T>();
	const size_t idx = m_read;
	assert(idx <= (m_cap - 1));
	m_read = (m_read + 1) % m_cap;
	m_len = (m_len > 0) ? m_len - 1 : 0;
	return Perhaps<T>(m_buf[idx]);
}

template<typename T>
Perhaps<T> Ring<T>::peek() const {
	if (m_len == 0) return Perhaps<T>();
	const size_t idx = m_read;
	assert(idx <= (m_cap - 1));
	return Perhaps<T>(m_buf[idx]);
}

template<typename T>
void Ring<T>::write(T item) {
	const size_t idx = m_write;
	assert(idx <= (m_cap - 1));
	m_write = (m_write + 1) % m_cap;
	m_len = (m_len < m_cap) ? m_len + 1 : m_cap;
	m_buf[idx] = item;
}

template<typename T>
void Ring<T>::write_many(T buf[], size_t len) {
	assert(len <= (m_cap - m_len));
	for (size_t i = 0; i < len; i++) write(buf[i]);
}

#endif

#endif
