# C++ Best Practices Guidelines

[Source](https://github.com/isocpp/CppCoreGuidelines)

## The structure of this document

Each rule (guideline, suggestion) can have several parts:

* The rule itself -- e.g., **no naked `new`**
* **Reason**s (rationales) -- because programmers find it hard to follow rules they don't understand
* **Example**s -- because rules are hard to understand in the abstract; can be positive or negative
* **Alternative**s -- for "don't do this" rules
* **Exception**s -- we prefer simple general rules. However, many rules apply widely, but not universally, so exceptions must be listed
* **Enforcement** -- ideas about how the rule might be checked "mechanically"
* **See also**s -- references to related rules and/or further discussion (in this document or elsewhere)
* **Note**s (comments) -- something that needs saying that doesn't fit the other classifications

Some rules are hard to check mechanically, but they all meet the minimal criteria that an expert programmer can spot many violations without too much trouble.
We hope that "mechanical" tools will improve with time to approximate what such an expert programmer notices.
Also, we assume that the rules will be refined over time to make them more precise and checkable.

Topics:
* [1. Compile time checking](#compiletimechecking)
* [2. Avoid non-const](#avoidnonconst)
* [3. Rule of zero](#ruleofzero)
* [4. Rule of five](#ruleoffive)
* [5. Raw pointer ownership transfer](#smartptr)
* [6. Choosing of smart pointer](#usingsmartptr)
* [7. Explicit resource allocation](#singlealloc)
* [8. Passing smart pointers](#passsmartptr)
* [9. Variable scope](#varlimitscope)
* [10. Variable recycling](#varrecycle)
* [11. Macros constants](#macrosconsts)
* [12. Magic constants](#magicconstants)
* [13. nullptr](#nullptr)
* [14. Narrowing](#construct)
* [15. Lambda, capture by reference](#referencecapture)
* [16. Lambda, capture by value](#valuecapture)
* [17. Lambda, capture this](#thiscapture)
* [18. Default constructor](#defaultctor)
* [19. Explicit constructor](#explicitctor)
* [20. Always initialize an object](#alwaysinitialize)
* [21. Use lambdas for complex initialization](#lambdainit)
* [22. Initialize class members](#orderctor)
* [23. In-class initializers](#inclassinitializer)
* [24. Prefer initialization to assignment](#initializetoassignment)
* [25. Named cast](#castsnamed)
* [26. Don't cast away `const`](#castsconst)
* [27. Symmetric operators](#symmetric)



### <a name="compiletimechecking"></a> 1. Prefer compile-time checking to run-time checking

##### Reason

Code clarity and performance.
You don't need to write error handlers for errors caught at compile time.

##### Example
```cpp
// Int is an alias used for integers
int bits {0};
for (Int i = 1; i; i <<= 1)
{
	++bits;
}
if (bits < 32)
{
	cerr << "Int too small\n";
}
```
This example fails to achieve what it is trying to achieve (because overflow is undefined) and should be replaced with a simple `static_assert`:
```cpp
// Int is an alias used for integers
static_assert(sizeof(Int) >= 4);    // do: compile-time check
```
Or better still just use the type system and replace `Int` with `int32_t`.

**Alternative formulation**: Don't postpone to run time what can be done well at compile time.

##### Enforcement

* Look for pointer arguments.
* Look for run-time checks for range violations.

### <a name="avoidnonconst"></a> 2. Avoid non-`const` global variables

##### Reason

Non-`const` global variables hide dependencies and make the dependencies subject to unpredictable changes.

##### Example
```cpp
struct Data
{
	// ... lots of stuff ...
} data;            // non-const data

void Compute()     // don't, hidden dependency to struct data
{
	// ... use data ...
}

void Output()     // don't, hidden dependency to struct data
{
	// ... use data ...
}
```
Who else might modify `data`?

##### Note

Global constants are useful.

##### Note

The rule against global variables applies to namespace scope variables as well.

**Alternative**: If you use global (more generally namespace scope) data to avoid copying, consider passing the data as an object by reference to `const`.
Another solution is to define the data as the state of some object and the operations as member functions.

**Warning**: Beware of data races: If one thread can access nonlocal data (or data passed by reference) while another thread executes the callee, we can have a data race.
Every pointer or reference to mutable data is a potential data race.

##### Note

You cannot have a race condition on immutable data.

##### Enforcement

(Simple) Report all non-`const` variables declared at namespace scope.

### <a name="ruleofzero"></a> 3. If you can avoid defining default operations, do

##### Reason
It's the simplest and gives the cleanest semantics.

##### Example
```cpp
class NamedMap
{
public:
	// ... no default operations declared ...
private:
	std::string mName;
	std::map<int, int> mRep;
};

NamedMap nm;        // default construct
NamedMap nm2 {nm};  // copy construct
```
Since `std::map` and `string` have all the special functions, no further work is needed.

##### Note

This is known as **"the rule of zero"**.

##### Enforcement

(Not enforceable) While not enforceable, a good static analyzer can detect patterns that indicate a possible improvement to meet this rule.
For example, a class with a (pointer, size) pair of member and a destructor that `delete`s the pointer could probably be converted to a `vector`.

### <a name="ruleoffive"></a> 4. If you define or `=delete` any default operation, define or `=delete` them all

##### Reason

The semantics of the special functions are closely related, so if one needs to be non-default, the odds are that others need modification too.

##### Example, bad
```cpp
class M2
{   // bad: incomplete set of default operations
public:
	// ...
	// ... no copy or move operations ...
	~M2() { delete[] mRep; }
private:
	std::pair<int, int>* mRep;  // zero-terminated set of pairs
};

void Use()
{
	M2 x;
	M2 y;
	// ...
	x = y;   // the default assignment
	// ...
}
```
Given that "special attention" was needed for the destructor (here, to deallocate), the likelihood that copy and move assignment (both will implicitly destroy an object) are correct is low (here, we would get double deletion).

##### Example, good
```cpp
class M2
{
public:
	M2(const std::size_t size);
	M2(const M2&) = delete; // or allocate and copy
	M2& operator=(const M2&) = delete; // or allocate and copy
	M2(M2&& rhs) = delete;  // or move and clear source pointer
	M2& operator=(M2&&) = delete; // or move and clear source pointer
	
	~M2() { delete[] mRep; }

private:
	std::pair<int, int>* mRep;  // zero-terminated set of pairs
};
```
##### Note

This is known as **"the rule of five"** or **"the rule of six"**, depending on whether you count the default constructor.

##### Note

If you want a default implementation of a default operation (while defining another), write `=default` to show you're doing so intentionally for that function.
If you don't want a default operation, suppress it with `=delete`.

##### Note

Compilers enforce much of this rule and ideally warn about any violation.

##### Note

Relying on an implicitly generated copy operation in a class with a destructor is deprecated.

##### Enforcement

(Simple) A class should have a declaration (even a `=delete` one) for either all or none of the special functions.

### <a name="smartptr"></a> 5. Never transfer ownership by a raw pointer (`T*`) or reference (`T&`)

##### Reason

If there is any doubt whether the caller or the callee owns an object, leaks or premature destruction will occur.

##### Example

Consider:
```cpp
X* Compute(args)    // don't, nobody knows whether pointer should be released
{
	X* res = new X{};
	// ...
	return res;
}
```
Who deletes the returned `X`? The problem would be harder to spot if compute returned a reference.
Consider returning the result by value (use move semantics if the result is large):
```cpp
vector<double> Compute(args)  // good
{
	vector<double> res(10000);
	// ...
	return res;
}
```
**Alternative**: Using a "smart pointer", such as `unique_ptr` (for exclusive ownership) and `shared_ptr` (for shared ownership).
However, that is less elegant and often less efficient than returning the object itself,
so use smart pointers only if reference semantics are needed.

**Alternative**: Sometimes older code can't be modified because of ABI compatibility requirements or lack of resources.
In that case, mark owning pointers using `owner` from the [guideline support library](#S-gsl):
```cpp
gsl::owner<X*> Compute(args)    // It is now clear that ownership is transferred
{
	gsl::owner<X*> res = new X{};
	// ...
	return res;
}
```
This tells analysis tools that `res` is an owner.
That is, its value must be `delete`d or transferred to another owner, as is done here by the `return`.

`owner` is used similarly in the implementation of resource handles.

##### Note

Every object passed as a raw pointer (or iterator) is assumed to be owned by the
caller, so that its lifetime is handled by the caller. Viewed another way:
ownership transferring APIs are relatively rare compared to pointer-passing APIs,
so the default is "no ownership transfer."

**See also**: [Using smart pointers](#usingsmartptr), [Use of smart pointer arguments](#passsmartptr).

##### Enforcement

* (Simple) Warn on `delete` of a raw pointer that is not an `owner<T>`. Suggest use of standard-library resource handle or use of `owner<T>`.
* (Simple) Warn on failure to either `reset` or explicitly `delete` an `owner` pointer on every code path.
* (Simple) Warn if the return value of `new` or a function call with an `owner` return value is assigned to a raw pointer or non-`owner` reference.

### <a name="usingsmartptr"></a> 6. Choose appropriate smart pointer or why we have more smart pointers?

##### Reason

Use appropriate smart pointer and avoid the waste of resources.

##### Example, bad
```cpp
void Foo()
{
	auto ptr = std::make_shared<MyBigClass>();
	// ...
	Use(*ptr); // may throw exception
	// ...
}
```
Shared pointer allocate reference counting even ownership is never transported or copied.

**Alternative**: If ownership owns only one object in a time, then use **`std::unique_ptr`** even that ownership can or will be moved to other object.
Using std::unique_ptr is the simplest way to avoid leaks. It is reliable, it makes the type system do much of the work to validate ownership safety,
it increases readability, and it has zero or near zero run-time cost.

**Alternative**: If ownership can be duplicated to other object then use **`std::shared_ptr`**.

**Note**: In both cases prefer creation via `std::make_unique` or `std::make_shared`.
If you first make an object and then give it to a shared_ptr constructor, you (most likely) do one more allocation (and later deallocation)
than if you use `std::make_shared` because the reference counts must be allocated separately from the object.
Use `make_unique` just for convenience and consistency with `std::shared_ptr`. `std::unique_ptr` shall be the default pointer type, `std::shared_ptr` shall be used only if needed.

##### See also

[Explicit resource allocation](#singlealloc)

[C++ Core Guidelines: Rules for Smart Pointers](http://www.modernescpp.com/index.php/c-core-guidelines-rules-to-smart-pointers)

### <a name="singlealloc"></a> 7. Perform at most one explicit resource allocation in a single expression statement

##### Reason

If you perform two explicit resource allocations in one statement, you could leak resources because the order of evaluation of many subexpressions, including function arguments, is unspecified.

##### Example

	void fun(shared_ptr<Widget> sp1, shared_ptr<Widget> sp2);

This `fun` can be called like this:
```cpp
// BAD: potential leak
fun(shared_ptr<Widget>(new Widget(a, b)), shared_ptr<Widget>(new Widget(c, d)));
```
This is exception-unsafe because the compiler may reorder the two expressions building the function's two arguments.
In particular, the compiler can interleave execution of the two expressions:
Memory allocation (by calling `operator new`) could be done first for both objects, followed by attempts to call the two `Widget` constructors.
If one of the constructor calls throws an exception, then the other object's memory will never be released!

This subtle problem has a simple solution: Never perform more than one explicit resource allocation in a single expression statement.
For example:
```cpp
	shared_ptr<Widget> sp1(new Widget(a, b)); // Better, but messy
	fun(sp1, new Widget(c, d));
```
The best solution is to avoid explicit allocation entirely use factory functions that return owning objects:
```cpp
	fun(make_shared<Widget>(a, b), make_shared<Widget>(c, d)); // Best
```
Write your own factory wrapper if there is not one already.

##### Enforcement

* Flag expressions with multiple explicit resource allocations (problem: how many direct resource allocations can we recognize?)

### <a name="passsmartptr"></a> 8. Take smart pointers as parameters only to explicitly express lifetime semantics

##### Reason

Accepting a smart pointer to a `widget` is wrong if the function just needs the `widget` itself.
It should be able to accept any `widget` object, not just ones whose lifetimes are managed by a particular kind of smart pointer.
A function that does not manipulate lifetime should take raw pointers or references instead.

##### Example, bad
```cpp
// callee
void Foo(std::shared_ptr<Widget>& w)
{
	// ...
	Use(*w); // only use of w -- the lifetime is not used at all
	// ...
};

// caller
std::shared_ptr<Widget> myWidget = /* ... */;
Foo(myWidget);

Widget stackWidget;
Foo(stackWidget); // error
```
##### Example, good
```cpp
// callee
void Foo(Widget& w)
{
	// ...
	Use(w);
	// ...
};

// caller
std::shared_ptr<Widget> myWidget = /* ... */;
Foo(*myWidget);

Widget stackWidget;
Foo(stackWidget); // ok -- now this works
```
##### Enforcement

* (Simple) Warn if a function takes a parameter of a smart pointer type (that overloads `operator->` or `operator*`) that is copyable but the function only calls any of: `operator*`, `operator->` or `get()`.
  Suggest using a `T*` or `T&` instead.
* Flag a parameter of a smart pointer type (a type that overloads `operator->` or `operator*`) that is copyable/movable but never copied/moved from in the function body, and that is never modified, and that is not passed along to another function that could do so. That means the ownership semantics are not used.
  Suggest using a `T*` or `T&` instead.

### <a name="varlimitscope"></a> 9. Declare names in for-statement initializers and conditions to limit scope

##### Reason

Readability. Minimize resource retention.

##### Example
```cpp
void use()
{
	for (string s; cin >> s;)
	{
		v.push_back(s);
	}

	for (int i = 0; i < 20; ++i)
	{   // good: i is local to for-loop
		// ...
	}

	if (auto pc = dynamic_cast<Circle*>(ps))
	{   // good: pc is local to if-statement
		// ... deal with Circle ...
	}
	else
	{
		// ... handle error ...
	}
}
```
##### Enforcement

* Flag loop variables declared before the loop and not used after the loop
* (hard) Flag loop variables declared before the loop and used after the loop for an unrelated purpose.

##### C++17 example

Note: C++17 also adds `if` and `switch` initializer statements. These require C++17 support.
```cpp
map<int, string> mymap;

if (auto result = mymap.insert(value); result.second)
{
	// insert succeeded, and result is valid for this block
	Use(result.first);  // ok
	// ...
} // result is destroyed here
```
##### C++17 enforcement (if using a C++17 compiler)

* Flag selection/loop variables declared before the body and not used after the body
* (hard) Flag selection/loop variables declared before the body and used after the body for an unrelated purpose.

### <a name="varrecycle"></a> 10. Don't use a variable for two unrelated purposes

##### Reason

Readability and safety.

##### Example, bad
```cpp
void Use()
{
	int i;
	for (i = 0; i < 20; ++i) { /* ... */ }
	for (i = 0; i < 200; ++i) { /* ... */ } // bad: i recycled
}
```
##### Note

As an optimization, you may want to reuse a buffer as a scratch pad, but even then prefer to limit the variable's scope as much as possible and be careful not to cause bugs from data left in a recycled buffer as this is a common source of security bugs.
```cpp
void Write() {
	std::string buffer;             // to avoid reallocations on every loop iteration
	for (auto& o : objects)
	{
		// First part of the work.
		generate_first_String(buffer, o);
		Write(buffer);

		// Second part of the work.
		generate_second_string(buffer, o);
		Write(buffer);

		// etc...
	}
}
```
##### Enforcement

Flag recycled variables.

### <a name="macrosconsts"></a> 11. Don't use macros for constants or "functions"

##### Reason

Macros are a major source of bugs.
Macros don't obey the usual scope and type rules.
Macros don't obey the usual rules for argument passing.
Macros ensure that the human reader sees something different from what the compiler sees.
Macros complicate tool building.

##### Example, bad
```cpp
#define PI 3.14
#define SQUARE(a, b) (a * b)
```
Even if we hadn't left a well-known bug in `SQUARE` there are much better behaved alternatives; for example:
```cpp
constexpr double pi = 3.14;
template<typename T> T square(T a, T b) { return a * b; }
```
##### Enforcement

Scream when you see a macro that isn't just used for source control (e.g., `#ifdef`)

### <a name="magicconstants"></a> 12. Avoid "magic constants"; use symbolic constants

##### Reason

Unnamed constants embedded in expressions are easily overlooked and often hard to understand:

##### Example
```cpp
for (int m = 1; m <= 12; ++m)   // don't: magic constant 12
	cout << month[m] << '\n';
```
No, we don't all know that there are 12 months, numbered 1..12, in a year. Better:
```cpp
// months are indexed 1..12
constexpr int first_month = 1;
constexpr int last_month = 12;

for (int m = first_month; m <= last_month; ++m)   // better
	cout << month[m] << '\n';
```
Better still, don't expose constants:
```cpp
for (auto m : month)
	cout << m << '\n';
```
##### Enforcement

Flag literals in code. Give a pass to `0`, `1`, `nullptr`, `\n`, `""`, and others on a positive list.

### <a name="nullptr"></a> 13. Use `nullptr` rather than `0` or `NULL`

##### Reason

Readability. Minimize surprises: `nullptr` cannot be confused with an
`int`. `nullptr` also has a well-specified (very restrictive) type, and thus
works in more scenarios where type deduction might do the wrong thing on `NULL`
or `0`.

##### Example

Consider:
```cpp
void f(int);
void f(char*);
f(0);         // call f(int)
f(nullptr);   // call f(char*)
```
##### Enforcement

Flag uses of `0` and `NULL` for pointers. The transformation may be helped by simple program transformation.

### <a name="construct"></a> 14. Use the `T{e}`notation for construction

##### Reason

The `T{e}` construction syntax makes it explicit that construction is desired.
The `T{e}` construction syntax doesn't allow narrowing.
`T{e}` is the only safe and general expression for constructing a value of type `T` from an expression `e`.
The casts notations `T(e)` and `(T)e` are neither safe nor general.

##### Example

For built-in types, the construction notation protects against narrowing and reinterpretation
```cpp
void use(char ch, int i, double d, char* p, long long lng)
{
	int x1 = int{ch};     // OK, but redundant
	int x2 = int{d};      // error: double->int narrowing; use a cast if you need to
	int x3 = int{p};      // error: pointer to->int; use a reinterpret_cast if you really need to
	int x4 = int{lng};    // error: long long->int narrowing; use a cast if you need to

	int y1 = int(ch);     // OK, but redundant
	int y2 = int(d);      // bad: double->int narrowing; use a cast if you need to
	int y3 = int(p);      // bad: pointer to->int; use a reinterpret_cast if you really need to
	int y4 = int(lng);    // bad: long->int narrowing; use a cast if you need to

	int z1 = (int)ch;     // OK, but redundant
	int z2 = (int)d;      // bad: double->int narrowing; use a cast if you need to
	int z3 = (int)p;      // bad: pointer to->int; use a reinterpret_cast if you really need to
	int z4 = (int)lng;    // bad: long long->int narrowing; use a cast if you need to
}
```
The integer to/from pointer conversions are implementation defined when using the `T(e)` or `(T)e` notations, and non-portable
between platforms with different integer and pointer sizes.

##### Note

When unambiguous, the `T` can be left out of `T{e}`.
```cpp
complex<double> f(complex<double>);

auto z = f({2*pi, 1});
```

##### Exception

`std::vector` and other containers were defined before we had `{}` as a notation for construction.
Consider:
```cpp
vector<string> vs {10};                           // ten empty strings
vector<int> vi1 {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};  // ten elements 1..10
vector<int> vi2 {10};                             // one element with the value 10
```
How do we get a `vector` of 10 default initialized `int`s?

	vector<int> v3(10); // ten elements with value 0

The use of `()` rather than `{}` for number of elements is conventional (going back to the early 1980s), hard to change, but still
a design error: for a container where the element type can be confused with the number of elements, we have an ambiguity that
must be resolved.
The conventional resolution is to interpret `{10}` as a list of one element and use `(10)` to distinguish a size.

This mistake need not be repeated in new code.
We can define a type to represent the number of elements:
```cpp
struct Count { int n; };

template<typename T>
class Vector
{
public:
	Vector(Count n);                     // n default-initialized elements
	Vector(initializer_list<T> init);    // init.size() elements
	// ...
};

Vector<int> v1{10};
Vector<int> v2{Count{10}};
Vector<Count> v3{Count{10}};    // yes, there is still a very minor problem
```
The main problem left is to find a suitable name for `Count`.

##### Enforcement

Flag the C-style `(T)e` and functional-style `T(e)` casts.


### <a name="referencecapture"></a> 15. Prefer capturing by reference in lambdas that will be used locally, including passed to algorithms

##### Reason

For efficiency and correctness, you nearly always want to capture by reference when using the lambda locally. This includes when writing or calling parallel algorithms that are local because they join before returning.

##### Discussion

The efficiency consideration is that most types are cheaper to pass by reference than by value.

The correctness consideration is that many calls want to perform side effects on the original object at the call site (see example below). Passing by value prevents this.

##### Note

Unfortunately, there is no simple way to capture by reference to `const` to get the efficiency for a local call but also prevent side effects.

##### Example

Here, a large object (a network message) is passed to an iterative algorithm, and is it not efficient or correct to copy the message (which may not be copyable):
```cpp
std::for_each(begin(sockets), end(sockets), [&message](auto& socket)
{
	socket.send(message);
});
```
##### Example

This is a simple three-stage parallel pipeline. Each `stage` object encapsulates a worker thread and a queue, has a `process` function to enqueue work, and in its destructor automatically blocks waiting for the queue to empty before ending the thread.
```cpp
void send_packets(buffers& bufs)
{
	stage encryptor([] (buffer& b){ encrypt(b); });
	stage compressor([&](buffer& b){ compress(b); encryptor.process(b); });
	stage decorator([&](buffer& b){ decorate(b); compressor.process(b); });
	for (auto& b : bufs) { decorator.process(b); }
}  // automatically blocks waiting for pipeline to finish
```
##### Enforcement

Flag a lambda that captures by reference, but is used other than locally within the function scope or passed to a function by reference. (Note: This rule is an approximation, but does flag passing by pointer as those are more likely to be stored by the callee, writing to a heap location accessed via a parameter, returning the lambda, etc. The Lifetime rules will also provide general rules that flag escaping pointers and references including via lambdas.)

### <a name="valuecapture"></a> 16. Avoid capturing by reference in lambdas that will be used nonlocally, including returned, stored on the heap, or passed to another thread

##### Reason

Pointers and references to locals shouldn't outlive their scope. Lambdas that capture by reference are just another place to store a reference to a local object, and shouldn't do so if they (or a copy) outlive the scope.

##### Example, bad
```cpp
int local{42};

// Want a reference to local.
// Note, that after program exits this scope,
// local no longer exists, therefore
// process() call will have undefined behavior!
thread_pool.queue_work([&]{ process(local); });
```
##### Example, good
```cpp
int local{42};
// Want a copy of local.
// Since a copy of local is made, it will
// always be available for the call.
thread_pool.queue_work([=]{ process(local); });
```
##### Enforcement

* (Simple) Warn when capture-list contains a reference to a locally declared variable
* (Complex) Flag when capture-list contains a reference to a locally declared variable and the lambda is passed to a non-`const` and non-local context

### <a name="thiscapture"></a> 17. If you capture `this`, capture all variables explicitly (no default capture)

##### Reason

It's confusing. Writing `[=]` in a member function appears to capture by value, but actually captures data members by reference because it actually captures the invisible `this` pointer by value. If you meant to do that, write `this` explicitly.

##### Example
```cpp
class MyClass
{
	int x = 0;
	// ...

	void f()
	{
		int i = 0;
		// ...

		auto lambda = [=]{ use(i, x); };   // BAD: "looks like" copy/value capture
		// [&] has identical semantics and copies the this pointer under the current rules
		// [=,this] and [&,this] are not much better, and confusing

		x = 42;
		lambda(); // calls use(0, 42);
		x = 43;
		lambda(); // calls use(0, 43);

		// ...

		auto lambda2 = [i, this]{ use(i, x); }; // ok, most explicit and least confusing

		// ...
	}
};
```
##### Note

This is under active discussion in standardization, and may be addressed in a future version of the standard by adding a new capture mode or possibly adjusting the meaning of `[=]`. For now, just be explicit.

##### Enforcement

* Flag any lambda capture-list that specifies a default capture and also captures `this` (whether explicitly or via default capture)

### <a name="defaultctor"></a> 18. Don't define a default constructor that only initializes data members; use in-class member initializers instead

##### Reason

Using in-class member initializers lets the compiler generate the function for you. The compiler-generated function can be more efficient.

##### Example, bad
```cpp
struct X1  // BAD: doesn't use member initializers
{
	X1() : s{"default"}, i{1}
	{ }
	// ...

	string s;
	int i;
};
```
##### Example
```cpp
class X2
{
	// use compiler-generated default constructor
	// ...

	string s{"default"};
	int i{1};
};
```
##### Enforcement

(Simple) A default constructor should do more than just initialize member variables with constants.

### <a name="explicitctor"></a> 19. By default, declare single-argument constructors explicit

##### Reason

To avoid unintended conversions.

##### Example, bad
```cpp
class String
{
	// ...
public:
	String(int);   // BAD
	// ...
};

String s = 10;   // surprise: string of size 10
```
##### Exception

If you really want an implicit conversion from the constructor argument type to the class type, don't use `explicit`:
```cpp
class Complex
{
public:
	Complex(double d);   // OK: we want a conversion from d to {d, 0}
	// ...
};

Complex z = 10.7;   // unsurprising conversion
```

##### Enforcement

(Simple) Single-argument constructors should be declared `explicit`. Good single argument non-`explicit` constructors are rare in most code based. Warn for all that are not on a "positive list".

### <a name="alwaysinitialize"></a> 20. Always initialize an object

##### Reason

Avoid used-before-set errors and their associated undefined behavior.
Avoid problems with comprehension of complex initialization.
Simplify refactoring.

##### Example
```cpp
void Use(int arg)
{
	int i;   // bad: uninitialized variable
	// ...
	i = 7;   // initialize i
}
```
No, `i = 7` does not initialize `i`; it assigns to it. Also, `i` can be read in the `...` part. Better:
```cpp
void Use(int arg)   // OK
{
	int i = 7;   // OK: initialized
	string s;    // OK: default initialized
	// ...
}
```
##### Note

The *always initialize* rule is deliberately stronger than the *an object must be set before used* language rule.
The latter, more relaxed rule, catches the technical bugs, but:

* It leads to less readable code
* It encourages people to declare names in greater than necessary scopes
* It leads to harder to read code
* It leads to logic bugs by encouraging complex code
* It hampers refactoring

The *always initialize* rule is a style rule aimed to improve maintainability as well as a rule protecting against used-before-set errors.

##### Example

Here is an example that is often considered to demonstrate the need for a more relaxed rule for initialization
```cpp
Widget i;    // "widget" a type that's expensive to initialize, possibly a large POD
Widget j;

if (cond)
{  // bad: i and j are initialized "late"
	i = f1();
	j = f2();
}
else
{
	i = f3();
	j = f4();
}
```
This cannot trivially be rewritten to initialize `i` and `j` with initializers.
Note that for types with a default constructor, attempting to postpone initialization simply leads to a default initialization followed by an assignment.
A popular reason for such examples is "efficiency", but a compiler that can detect whether we made a used-before-set error can also eliminate any redundant double initialization.

At the cost of repeating `cond` we could write:
```cpp
Widget i = (cond) ? f1() : f3();
Widget j = (cond) ? f2() : f4();
```
Assuming that there is a logical connection between `i` and `j`, that connection should probably be expressed in code:
```cpp
std::pair<Widget, Widget> MakeRelatedWidgets(bool x)
{
	return (x) ? {f1(), f2()} : {f3(), f4() };
}

auto init = MakeRelatedWidgets(cond);
Widget i = init.first;
Widget j = init.second;
```
Obviously, what we really would like is a construct that initialized n variables from a `tuple`. For example:
```cpp
auto [i, j] = MakeRelatedWidgets(cond);    // C++17, not C++14
```
Today, we might approximate that using `tie()`:
```cpp
Widget i;       // bad: uninitialized variable
Widget j;
std::tie(i, j) = MakeRelatedWidgets(cond);
```
This may be seen as an example of the *immediately initialize from input* exception below.

Creating optimal and equivalent code from all of these examples should be well within the capabilities of modern C++ compilers
(but don't make performance claims without measuring; a compiler may very well not generate optimal code for every example and
there may be language rules preventing some optimization that you would have liked in a particular case).

##### Example

This rule covers member variables.
```cpp
class X
{
public:
	X(int i, int i2) : m2{i}, m5{i2} {}
	// ...

private:
	int m1 = 7;
	int m2;
	int m3;

	const int m4 = 7;
	const int m5;
	const int m6;
};
```
The compiler will flag the uninitialized `m6` because it is a `const`, but it will not catch the lack of initialization of `m3`.
Usually, a rare spurious member initialization is worth the absence of errors from lack of initialization and often an optimizer
can eliminate a redundant initialization (e.g., an initialization that occurs immediately before an assignment).

##### Note

Complex initialization has been popular with clever programmers for decades.
It has also been a major source of errors and complexity.
Many such errors are introduced during maintenance years after the initial implementation.

##### Exception

If you are declaring an object that is just about to be initialized from input, initializing it would cause a double initialization.
However, beware that this may leave uninitialized data beyond the input -- and that has been a fertile source of errors and security breaches:
```cpp
constexpr int max = 8 * 1024;
int buf[max];         // OK, but suspicious: uninitialized
f.read(buf, max);
```
The cost of initializing that array could be significant in some situations.
However, such examples do tend to leave uninitialized variables accessible, so they should be treated with suspicion.
```cpp
constexpr int max = 8 * 1024;
int buf[max] = {};   // zero all elements; better in some situations
f.read(buf, max);
```
When feasible use a library function that is known not to overflow. For example:
```cpp
std::string s;   // s is default initialized to ""
cin >> s;   // s expands to hold the string
```
Don't consider simple variables that are targets for input operations exceptions to this rule:
```cpp
int i;   // bad
// ...
std::cin >> i;
```
In the not uncommon case where the input target and the input operation get separated (as they should not) the possibility of used-before-set opens up.
```cpp
int i2 = 0;   // better
// ...
std::cin >> i;
```
A good optimizer should know about input operations and eliminate the redundant operation.

##### Example

Using an `uninitialized` or sentinel value is a symptom of a problem and not a
solution:
```cpp
Widget i = uninit;  // bad
Widget j = uninit;

// ...
Use(i);         // possibly used before set
// ...

if (cond)
{     // bad: i and j are initialized "late"
	i = f1();
	j = f2();
}
else
{
	i = f3();
	j = f4();
}
```
Now the compiler cannot even simply detect a used-before-set. Further, we've introduced complexity in the state space for widget: which operations are valid on an `uninit` widget and which are not?

##### Note

Sometimes, a lambda can be used as an initializer to avoid an uninitialized variable:
```cpp
error_code ec;
Value v = [&] {
	auto p = GetValue();   // GetValue() returns a pair<error_code, Value>
	ec = p.first;
	return p.second;
}();
```
or maybe:
```cpp
Value v = [] {
	auto p = GetValue();   // GetValue() returns a pair<error_code, Value>
	if (p.first) throw BadValue{p.first};
	return p.second;
}();
```
**See also**: [Use lambdas for complex initialization](#lambdainit)

##### Enforcement

* Flag every uninitialized variable.
  Don't flag variables of user-defined types with default constructors.
* Check that an uninitialized buffer is written into *immediately* after declaration.
  Passing an uninitialized variable as a reference to non-`const` argument can be assumed to be a write into the variable.

### <a name="lambdainit"></a> 21. Use lambdas for complex initialization, especially of `const` variables

##### Reason

It nicely encapsulates local initialization, including cleaning up scratch variables needed only for the initialization, without needing to create a needless nonlocal yet nonreusable function. It also works for variables that should be `const` but only after some initialization work.

##### Example, bad
```cpp
Widget x;   // should be const, but:
for (auto i = 2; i <= N; ++i)              // this could be some
{
	x += some_obj.do_something_with(i);  // arbitrarily long code
}                                        // needed to initialize x
// from here, x should be const, but we can't say so in code in this style
```
##### Example, good
```cpp
const Widget x = [&]
{
	Widget val;                                // assume that widget has a default constructor
	for (auto i = 2; i <= N; ++i) {            // this could be some
		val += some_obj.do_something_with(i);  // arbitrarily long code
	}                                          // needed to initialize x
	return val;
}();
```
##### Example
```cpp
std::string var = [&]
{
	if (!in)
	{
		return "";   // default
	}
	std::string s;
	for (char c : in >> c)
		s += toupper(c);
	return s;
}(); // note ()
```
If at all possible, reduce the conditions to a simple set of alternatives (e.g., an `enum`) and don't mix up selection and initialization.

##### Enforcement

Hard. At best a heuristic. Look for an uninitialized variable followed by a loop assigning to it.

### <a name="orderctor"></a> 22. Define and initialize member variables in the order of member declaration

##### Reason

To minimize confusion and errors. That is the order in which the initialization happens (independent of the order of member initializers).

##### Example, bad
```cpp
class Foo
{
public:
	Foo(int x) :m2{x}, m1{++x} { }   // BAD: misleading initializer order
	// ...

private:
	int m1;
	int m2;
};

Foo x(1); // surprise: x.m1 == x.m2 == 2
```
##### Enforcement

(Simple) A member initializer list should mention the members in the same order they are declared.

### <a name="inclassinitializer"></a> 23. Prefer in-class initializers to member initializers in constructors for constant initializers

##### Reason

Makes it explicit that the same value is expected to be used in all constructors. Avoids repetition. Avoids maintenance problems. It leads to the shortest and most efficient code.

##### Example, bad
```cpp
class X    // BAD
{
public:
	X() :mI{666}, mS{"qqq"} { }   // j is uninitialized
	X(int ii) :mI{ii} {}         // s is "" and j is uninitialized
	// ...

private:
	int mI;
	string mS;
	int mJ;
};
```
How would a maintainer know whether `mJ` was deliberately uninitialized (probably a poor idea anyway) and whether it was intentional to give `mS` the default value `""`
in one case and `qqq` in another (almost certainly a bug)? The problem with `mJ` (forgetting to initialize a member) often happens when a new member is added to an existing class.

##### Example
```cpp
class X2
{
public:
	X2() = default;        // all members are initialized to their defaults
	X2(int ii) :mI{ii} {}   // s and j initialized to their defaults
	// ...

private:
	int mI {666};
	string mS {"qqq"};
	int mJ {0};
};
```

**Alternative**: We can get part of the benefits from default arguments to constructors, and that is not uncommon in older code. However, that is less explicit,
causes more arguments to be passed, and is repetitive when there is more than one constructor:

```cpp
class X3
{   // BAD: inexplicit, argument passing overhead
public:
	X3(int ii = 666, const string& ss = "qqq", int jj = 0)
		:mI{ii}, mS{ss}, mJ{jj} { }   // all members are initialized to their defaults
	// ...

private:
	int mI;
	string mS;
	int mJ;
};
```
##### Enforcement

* (Simple) Every constructor should initialize every member variable (either explicitly, via a delegating ctor call or via default construction).
* (Simple) Default arguments to constructors suggest an in-class initializer may be more appropriate.

### <a name="initializetoassignment"></a> 24. Prefer initialization to assignment in constructors

##### Reason

An initialization explicitly states that initialization, rather than assignment, is done and can be more elegant and efficient. Prevents "use before set" errors.

##### Example, good
```cpp
class A
{   // Good
public:
	A() : mS1{"Hello, "} { }    // GOOD: directly construct
	// ...

private:
	string mS1;
};
```
##### Example, bad
```cpp
class B
{   // BAD
public:
	B() { mS1 = "Hello, "; }   // BAD: default constructor followed by assignment
	// ...

private:
	string mS1;
};

class C
{   // UGLY, aka very bad
public:
	C() { cout << *mP; mP = new int{10}; }   // accidental use before initialized
	// ...

private:
	int* mP;
};
```

### <a name="castsnamed"></a> 25. If you must use a cast, use a named cast

##### Reason

Readability. Error avoidance.
Named casts are more specific than a C-style or functional cast, allowing the compiler to catch some errors.

The named casts are:

* `static_cast`
* `const_cast`
* `reinterpret_cast`
* `dynamic_cast`
* `std::move`         // `move(x)` is an rvalue reference to `x`
* `std::forward`      // `forward(x)` is an rvalue reference to `x`
* `gsl::narrow_cast`  // `narrow_cast<T>(x)` is `static_cast<T>(x)`
* `gsl::narrow`       // `narrow<T>(x)` is `static_cast<T>(x)` if `static_cast<T>(x) == x` or it throws `narrowing_error`

##### Example
```cpp
class B { /* ... */ };
class D { /* ... */ };

template<typename D> D* upcast(B* pb)
{
	D* pd0 = pb;                        // error: no implicit conversion from B* to D*
	D* pd1 = (D*)pb;                    // legal, but what is done?
	D* pd2 = static_cast<D*>(pb);       // error: D is not derived from B
	D* pd3 = reinterpret_cast<D*>(pb);  // OK: on your head be it!
	D* pd4 = dynamic_cast<D*>(pb);      // OK: return nullptr
	// ...
}
```
The example was synthesized from real-world bugs where `D` used to be derived from `B`, but someone refactored the hierarchy.
The C-style cast is dangerous because it can do any kind of conversion, depriving us of any protection from mistakes (now or in the future).

##### Note

When converting between types with no information loss (e.g. from `float` to
`double` or `int64` from `int32`), brace initialization may be used instead.
```cpp
double d {some_float};
int64_t i {some_int32};
```
This makes it clear that the type conversion was intended and also prevents
conversions between types that might result in loss of precision. (It is a
compilation error to try to initialize a `float` from a `double` in this fashion,
for example.)

##### Note

`reinterpret_cast` can be essential, but the essential uses (e.g., turning a machine address into pointer) are not type safe:
```cpp
auto p = reinterpret_cast<Device_register>(0x800);  // inherently dangerous
```

##### Enforcement

* Flag C-style and functional casts.
* The [type profile](#Pro-type-reinterpretcast) bans `reinterpret_cast`.
* The [type profile](#Pro-type-arithmeticcast) warns when using `static_cast` between arithmetic types.

### <a name="castsconst"></a> 26. Don't cast away `const`

##### Reason

It makes a lie out of `const`.
If the variable is actually declared `const`, the result of "casting away `const`" is undefined behavior.

##### Example, bad
```cpp
void f(const int& i)
{
	const_cast<int&>(i) = 42;   // BAD
}

static int i = 0;
static const int j = 0;

f(i); // silent side effect
f(j); // undefined behavior
```
##### Example

Sometimes, you may be tempted to resort to `const_cast` to avoid code duplication, such as when two accessor functions that differ only in `const`-ness have similar implementations. For example:
```cpp
class Bar;

class Foo
{
public:
	// BAD, duplicates logic
	Bar& GetBar() {
		/* complex logic around getting a non-const reference to mBar */
	}

	const Bar& GetBar() const {
		/* same complex logic around getting a const reference to mBar */
	}
private:
	Bar mBar;
};
```
Instead, prefer to share implementations. Normally, you can just have the non-`const` function call the `const` function. However, when there is complex logic this can lead to the following pattern that still resorts to a `const_cast`:
```cpp
class Foo
{
public:
	// not great, non-const calls const version but resorts to const_cast
	Bar& GetBar() {
		return const_cast<Bar&>(static_cast<const Foo&>(*this).GetBar());
	}
	const Bar& GetBar() const {
		/* the complex logic around getting a const reference to mBar */
	}
private:
	Bar mBar;
};
```
Although this pattern is safe when applied correctly, because the caller must have had a non-`const` object to begin with, it's not ideal because the safety is hard to enforce automatically as a checker rule.

Instead, prefer to put the common code in a common helper function -- and make it a template so that it deduces `const`. This doesn't use any `const_cast` at all:
```cpp
class Foo
{
public:                         // good
			Bar& GetBar()       { return GetBarImpl(*this); }
	const Bar& GetBar() const { return GetBarImpl(*this); }
private:
	Bar mBar;

	template<class T>           // good, deduces whether T is const or non-const
	static auto GetBarImpl(T& t) -> decltype(t.GetBar())
		{ /* the complex logic around getting a possibly-const reference to mBar */ }
};
```
##### Exception

You may need to cast away `const` when calling `const`-incorrect functions.
Prefer to wrap such functions in inline `const`-correct wrappers to encapsulate the cast in one place.

##### Example

Sometimes, "cast away `const`" is to allow the updating of some transient information of an otherwise immutable object.
Examples are caching, memoization, and precomputation.
Such examples are often handled as well or better using `mutable` or an indirection than with a `const_cast`.

Consider keeping previously computed results around for a costly operation:
```cpp
int Compute(int x); // compute a value for x; assume this to be costly

class Cache
{   // some type implementing a cache for an int->int operation
public:
	pair<bool, int> Find(int x) const;   // is there a value for x?
	void Set(int x, int v);             // make y the value for x
	// ...
private:
	// ...
};

class X
{
public:
	int GetVal(int x)
	{
		auto p = cache.Find(x);
		if (p.first) return p.second;
		int val = Compute(x);
		cache.Set(x, val); // insert value for x
		return val;
	}
	// ...
private:
	Cache cache;
};
```
Here, `GetVal()` is logically constant, so we would like to make it a `const` member.
To do this we still need to mutate `cache`, so people sometimes resort to a `const_cast`:
```cpp
class X
{   // Suspicious solution based on casting
public:
	int GetVal(int x) const
	{
		auto p = cache.Find(x);
		if (p.first) return p.second;
		int val = Compute(x);
		const_cast<Cache&>(cache).Set(x, val);   // ugly
		return val;
	}
	// ...
private:
	Cache cache;
};
```
Fortunately, there is a better solution:
State that `cache` is mutable even for a `const` object:
```cpp
class X
{   // better solution
public:
	int GetVal(int x) const
	{
		auto p = cache.Find(x);
		if (p.first) return p.second;
		int val = Compute(x);
		cache.Set(x, val);
		return val;
	}
	// ...
private:
	mutable Cache cache;
};
```
An alternative solution would to store a pointer to the `cache`:
```cpp
class X
{   // OK, but slightly messier solution
public:
	int GetVal(int x) const
	{
		auto p = cache->Find(x);
		if (p.first) return p.second;
		int val = Compute(x);
		cache->Set(x, val);
		return val;
	}
	// ...
private:
	unique_ptr<Cache> cache;
};
```
That solution is the most flexible, but requires explicit construction and destruction of `*cache`
(most likely in the constructor and destructor of `X`).

In any variant, we must guard against data races on the `cache` in multi-threaded code, possibly using a `std::mutex`.

##### Enforcement

* Flag `const_cast`s.
* This rule is part of the [type-safety profile](#Pro-type-constcast) for the related Profile.

### <a name="symmetric"></a> 27. Use nonmember functions for symmetric operators

##### Reason

If you use member functions, you need two.
Unless you use a nonmember function for (say) `==`, `a == b` and `b == a` will be subtly different.

##### Example
```cpp
bool operator==(Point a, Point b) { return a.x == b.x && a.y == b.y; }
```

##### Note
*When to use a normal, friend, or member function overload?*

In most cases, the language leaves it up to you to determine whether you want to use the normal/friend or member function version of the overload.
However, one of the two is usually a better choice than the other.
When dealing with binary operators that don't modify the left operand (e.g. operator+), the normal or friend function version is typically preferred, because it works for all parameter types (even when the left operand isn't a class object, or is a class that is not modifiable).
The normal or friend function version has the added benefit of "symmetry", as all operands become explicit parameters (instead of the left operand becoming *this and the right operand becoming an explicit parameter).
When dealing with binary operators that do modify the left operand (e.g. operator+=), the member function version is typically preferred.
In these cases, the leftmost operand will always be a class type, and having the object being modified become the one pointed to by *this is natural.
Because the rightmost operand becomes an explicit parameter, there's no confusion over who is getting modified and who is getting evaluated.
Unary operators are usually overloaded as member functions as well, since the member version has no parameters.

The following rules of thumb can help you determine which form is best for a given situation:

- If you're overloading assignment (=), subscript ([]), function call (()), or member selection (->), do so as a **member function**.
- If you're overloading a unary operator, do so as a **member function**.
- If you're overloading a binary operator that modifies its left operand (e.g. operator+=), do so as a **member function** if you can.
- If you're overloading a binary operator that does not modify its left operand (e.g. operator+), do so as a **normal function** or **friend function**.

> **Not everything can be overloaded as a friend function**
> The assignment (=), subscript ([]), function call (()), and member selection (->) operators must be overloaded as member functions, because the language requires them to be.

> **Not everything can be overloaded as a member function**
> Overloading the I/O operators, we overloaded operator<< for our Point class using the friend function method.

##### Enforcement

Flag member operator functions.
