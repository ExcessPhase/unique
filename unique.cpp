#include <mutex>
#include <set>
#include <atomic>
#include <optional>
#include <type_traits>
#include <boost/intrusive_ptr.hpp>


	/// a nullmutex in case of a single threaded environment
class NullMutex
{	public:
	void lock(void)
	{
	}
	void unlock(void)
	{
	}
	bool try_lock(void)
	{	return true;
	}
};

	/// the template class to implement the BASE class
	/// look for example usage below
	/// default is multithreaded using std::recursive_mutex
template<typename T, bool BTHREADED = true>
class unique
{	typedef typename std::conditional<
		BTHREADED,
		std::recursive_mutex,
		NullMutex
	>::type MUTEX;
		/// does not need to be std::atomic as protected by a mutex
	mutable std::size_t m_sRefCount;
	public:
	unique(void)
		:m_sRefCount(std::size_t())
	{
	}
		/// for implementing the static set of registered pointers
	struct compare
	{	bool operator()(const T*const _p0, const T*const _p1) const
		{	return *_p0 < *_p1;
		}
	};
	private:
	typedef std::set<const T*, compare> SET;
	typedef std::pair<SET, MUTEX> setAndMutex;
	static setAndMutex&getSet(void)
	{	static setAndMutex s;
		return s;
	}
	public:
		/// the factory method
	template<typename DERIVED, typename ...ARGS>
	static boost::intrusive_ptr<const T> create(ARGS&&..._r)
	{	const auto s = boost::intrusive_ptr<const T>(new DERIVED(std::forward<ARGS>(_r)...));
		std::lock_guard<MUTEX> sLock(getSet().second);
		return *getSet().first.insert(s.get()).first;
	}
	private:
		/// called by boost::intrusive_ptr<const T>
	friend void intrusive_ptr_add_ref(const T* const _p) noexcept
	{	std::lock_guard<MUTEX> sLock(getSet().second);
		++_p->m_sRefCount;
	}
		/// called by boost::intrusive_ptr<const T>
	friend void intrusive_ptr_release(const T* const _p) noexcept
	{	std::optional<std::lock_guard<MUTEX> > sLock(getSet().second);
		if (!--_p->m_sRefCount)
		{	getSet().first.erase(_p);
			sLock.reset();
			delete _p;
		}
	}
};

#include <typeinfo>

	/// an example hierarchy base class
struct expression:unique<expression>
{	virtual ~expression(void) = default;
		/// for the set of pointers
		/// sorting only by typeinfo
	virtual bool operator<(const expression&_r) const
	{	return typeid(*this).before(typeid(_r));
	}
};
	/// one example derived expression
struct integerConstant:expression
{	const int m_i;
	integerConstant(const int _i)
		:m_i(_i)
	{
	}
	bool operator<(const expression&_r) const override
	{	if (this->expression::operator<(_r))
			return true;
		else
		if (_r.expression::operator<(*this))
			return false;
		else
			return m_i < static_cast<const integerConstant&>(_r).m_i;
	}
};
	/// one example derived expression
struct realConstant:expression
{	const double m_d;
	realConstant(const double _d)
		:m_d(_d)
	{
	}
	bool operator<(const expression&_r) const override
	{	if (this->expression::operator<(_r))
			return true;
		else
		if (_r.expression::operator<(*this))
			return false;
		else
			return m_d < static_cast<const realConstant&>(_r).m_d;
	}
};


#include <vector>
#include <thread>
#include <iostream>


int main(int argc, char**argv)
{	if (argc != 3)
	{	std::cerr << argv[0] << ": Error: Usage: " << argv[0] << " numberOfObjects numberOfThreads" << std::endl;
		return 1;
	}
		/// all the threads are doing the same
		/// creating a local vector of pointers
	const auto sCreate = [&](void)
	{	std::vector<boost::intrusive_ptr<const expression> > sVector;
		sVector.reserve(std::atoi(argv[1]));
		for (int i = 0; i < sVector.capacity(); ++i)
			sVector.emplace_back(
				i & 1
				? unique<expression>::create<integerConstant>(i)
				: unique<expression>::create<realConstant>(i*1.1)
			);
	};
		/// the thread objects
	std::vector<std::thread> sThreads;
	sThreads.reserve(std::size_t(std::atoi(argv[2])));
		/// starting the threads
	for (std::size_t i = 0; i < sThreads.capacity(); ++i)
		sThreads.emplace_back(sCreate);
		/// and waiting for the threads to terminate
	while (!sThreads.empty())
	{	sThreads.back().join();
		sThreads.pop_back();
	}
}
