//// HEADER-ONLY LIBRARY. To include impl define PERHAPS_IMPL

#ifndef PERHAPS_HPP
#define PERHAPS_HPP

#include <cassert>
#include <cstddef>

template<typename T>
struct Perhaps {
	// None constructor
	Perhaps();

	// Some constructors
	Perhaps(T& t);
	Perhaps(T&& t);

	Perhaps(const Perhaps<T>& other);
	Perhaps(Perhaps<T>&& other) noexcept;
	~Perhaps();

	Perhaps<T>& operator=(const Perhaps<T>& other);
	Perhaps<T>& operator=(Perhaps<T>&& other) noexcept;

	bool operator==(Perhaps<T>& other);

	bool is_some();
	bool is_none();
	T unwrap();

 private:
	bool m_checked {false}; // enforce checking if there's actually any value
	bool m_some;

	union {
		T m_value;
	};
};

#if defined PERHAPS_IMPL && !defined PERHAPS_IMPL_ALREADY_INCLUDED
#	define PERHAPS_IMPL_ALREADY_INCLUDED

/// CONSTRUCTORS AND DESTRUCTORS
template<typename T>
Perhaps<T>::Perhaps() : m_some(false) {}

template<typename T>
Perhaps<T>::Perhaps(T& t) : m_some(true) {
	new (&m_value) T(t);
}

template<typename T>
Perhaps<T>::Perhaps(T&& t) : m_some(true) {
	new (&m_value) T(std::move(t));
}

template<typename T>
Perhaps<T>::Perhaps(const Perhaps<T>& other) : m_some(other.m_some) {
	if (other.m_some) new (&m_value) T(other.m_value);
}

template<typename T>
Perhaps<T>::Perhaps(Perhaps<T>&& other) noexcept : m_some(other.m_some) {
	if (other.m_some) new (&m_value) T(other.m_value);
}

template<typename T>
Perhaps<T>::~Perhaps() {
	if (m_some) m_value.~T();
}

template<typename T>
Perhaps<T>& Perhaps<T>::operator=(Perhaps<T>&& other) noexcept {
	m_some = other.m_some;
	if (other.m_some) new (&m_value) T(other.m_value);
	return *this;
}

template<typename T>
Perhaps<T>& Perhaps<T>::operator=(const Perhaps<T>& other) {
	if (this == &other) return *this;
	m_some = other.m_some;
	if (other.m_some) new (&m_value) T(other.m_value);
	return *this;
}

/// OPERATORS
template<typename T>
bool Perhaps<T>::operator==(Perhaps<T>& other) {
	if (other.is_none() && is_none()) return true;
	if (other.is_none()) return false;
	if (is_none()) return false;
	return unwrap() == other.unwrap();
}

/// FUNCTIONS
template<typename T>
bool Perhaps<T>::is_some() {
	m_checked = true;
	return m_some;
}

template<typename T>
bool Perhaps<T>::is_none() {
	m_checked = true;
	return !m_some;
}

template<typename T>
T Perhaps<T>::unwrap() {
	assert(m_checked && m_some);
	return m_value;
}

#endif

#endif
