//
// Copyright (c) 2013-2016 Pavel Medvedev. All rights reserved.
//
// This file is part of v8pp (https://github.com/pmed/v8pp) project.
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "v8pp/class.hpp"
#include "v8pp/property.hpp"

#include "test.hpp"

struct Xbase
{
	int var = 1;

	int get() const { return var; }
	void set(int v) { var = v; }

	int prop() const { return var; }
	void prop(int v) { var = v; }

	int fun1(int x) { return var + x; }
	int fun2(int x) const { return var + x; }
	int fun3(int x) volatile { return var + x; }
	int fun4(int x) const volatile { return var + x; }
	static int static_fun(int x) { return x; }
};

struct X : Xbase
{
};

template<typename Traits, typename X_ptr = typename v8pp::class_<X, Traits>::object_pointer_type>
static X_ptr create_X(v8::FunctionCallbackInfo<v8::Value> const& args)
{
	return X_ptr(new X);
}

struct Y : X
{
	static int instance_count;

	explicit Y(int x) { var = x; ++instance_count; }
	~Y() { --instance_count; }

	int useX(X& x) { return var + x.var; }

	template<typename Traits, typename X_ptr = typename v8pp::class_<X, Traits>::object_pointer_type>
	int useX_ptr(X_ptr x) { return var + x->var; }
};

int Y::instance_count = 0;

struct Z {};

template<typename Traits>
static int extern_fun(v8::FunctionCallbackInfo<v8::Value> const& args)
{
	int x = args[0]->Int32Value();
	auto self = v8pp::class_<X, Traits>::unwrap_object(args.GetIsolate(), args.This());
	if (self) x += self->var;
	return x;
}

template<typename Traits>
void test_class_()
{
	Y::instance_count = 0;

	v8pp::context context;
	v8::Isolate* isolate = context.isolate();
	v8::HandleScope scope(isolate);

	using x_prop_get = int (X::*)() const;
	using x_prop_set = void (X::*)(int);

	v8pp::class_<X, Traits> X_class(isolate);
	X_class
		.ctor(&create_X<Traits>)
		.const_("konst", 99)
		.var("var", &X::var)
		.property("rprop", &X::get)
		.property("wprop", &X::get, &X::set)
		.property("wprop2", static_cast<x_prop_get>(&X::prop), static_cast<x_prop_set>(&X::prop))
//TODO:		.property("lprop", [](X const& x) { return x.var; }, [](X& x, int n) { x.var = n; })
		.function("fun1", &X::fun1)
		.function("fun2", &X::fun2)
		.function("fun3", &X::fun3)
		.function("fun4", &X::fun4)
		.function("static_fun", &X::static_fun)
		.function("static_lambda", [](int x) { return x + 3; })
		.function("extern_fun", extern_fun<Traits>)
		;

	v8pp::class_<Y, Traits> Y_class(isolate);
	Y_class
		.template inherit<X>()
		.template ctor<int>()
		.function("useX", &Y::useX)
		.function("useX_ptr", &Y::useX_ptr<Traits>)
		;

	check_ex<std::runtime_error>("already wrapped class X", [isolate]()
	{
		v8pp::class_<X, Traits> X_class(isolate);
	});
	check_ex<std::runtime_error>("already inherited class X", [isolate, &Y_class]()
	{
		Y_class.template inherit<X>();
	});
	check_ex<std::runtime_error>("unwrapped class Z", [isolate]()
	{
		v8pp::class_<Z, Traits>::find_object(isolate, nullptr);
	});

	context
		.class_("X", X_class)
		.class_("Y", Y_class)
		;

	check_eq("X object", run_script<int>(context, "x = new X(); x.var += x.konst"), 100);
	check_eq("X::rprop", run_script<int>(context, "x = new X(); x.rprop"), 1);
	check_eq("X::wprop", run_script<int>(context, "x = new X(); ++x.wprop"), 2);
	check_eq("X::wprop2", run_script<int>(context, "x = new X(); ++x.wprop2"), 2);
	check_eq("X::fun1(1)", run_script<int>(context, "x = new X(); x.fun1(1)"), 2);
	check_eq("X::fun2(2)", run_script<int>(context, "x = new X(); x.fun2(2)"), 3);
	check_eq("X::fun3(3)", run_script<int>(context, "x = new X(); x.fun3(3)"), 4);
	check_eq("X::fun4(4)", run_script<int>(context, "x = new X(); x.fun4(4)"), 5);
	check_eq("X::static_fun(1)", run_script<int>(context, "X.static_fun(1)"), 1);
	check_eq("X::static_lambda(1)", run_script<int>(context, "X.static_lambda(1)"), 4);
	check_eq("X::extern_fun(5)", run_script<int>(context, "x = new X(); x.extern_fun(5)"), 6);
	check_eq("X::extern_fun(6)", run_script<int>(context, "X.extern_fun(6)"), 6);

	check_eq("Y object", run_script<int>(context, "y = new Y(-100); y.konst + y.var"), -1);

	auto y1 = Traits::template create<Y>(-1);

	v8::Handle<v8::Object> y1_obj =
		v8pp::class_<Y, Traits>::reference_external(context.isolate(), y1);
	check("y1", v8pp::from_v8<decltype(y1)>(isolate, y1_obj) == y1);
	check("y1_obj", v8pp::to_v8(isolate, y1) == y1_obj);

	auto y2 = Traits::template create<Y>(-2);
	v8::Handle<v8::Object> y2_obj =
		v8pp::class_<Y, Traits>::import_external(context.isolate(), y2);
	check("y2", v8pp::from_v8<decltype(y2)>(isolate, y2_obj) == y2);
	check("y2_obj", v8pp::to_v8(isolate, y2) == y2_obj);

	v8::Handle<v8::Object> y3_obj =
		v8pp::class_<Y, Traits>::create_object(context.isolate(), -3);
	auto y3 = v8pp::class_<Y, Traits>::unwrap_object(isolate, y3_obj);
	check("y3", v8pp::from_v8<decltype(y3)>(isolate, y3_obj) == y3);
	check("y3_obj", v8pp::to_v8(isolate, y3) == y3_obj);
	check_eq("y3.var", y3->var, -3);

	run_script<int>(context, "x = new X; for (i = 0; i < 10; ++i) { y = new Y(i); y.useX(x); y.useX_ptr(x); }");
	check_eq("Y count", Y::instance_count, 10 + 4); // 10 + y + y1 + y2 + y3
	run_script<int>(context, "y = null; 0");

	v8pp::class_<Y, Traits>::unreference_external(isolate, y1);
	check("unref y1", !v8pp::from_v8<decltype(y1)>(isolate, y1_obj));
	check("unref y1_obj", v8pp::to_v8(isolate, y1).IsEmpty());
	y1_obj.Clear();
	check_ex<std::runtime_error>("y1 unreferenced", [isolate, &y1]()
	{
		v8pp::to_v8(isolate, y1);
	});

	v8pp::class_<Y, Traits>::destroy_object(isolate, y2);
	check("unref y2", !v8pp::from_v8<decltype(y2)>(isolate, y2_obj));
	check("unref y2_obj", v8pp::to_v8(isolate, y2).IsEmpty());
	y2_obj.Clear();

	v8pp::class_<Y, Traits>::destroy_object(isolate, y3);
	check("unref y3", !v8pp::from_v8<decltype(y3)>(isolate, y3_obj));
	check("unref y3_obj", v8pp::to_v8(isolate, y3).IsEmpty());
	y3_obj.Clear();

	std::string const v8_flags = "--expose_gc";
	v8::V8::SetFlagsFromString(v8_flags.data(), (int)v8_flags.length());
	context.isolate()->RequestGarbageCollectionForTesting(
		v8::Isolate::GarbageCollectionType::kFullGarbageCollection);

	bool const use_shared_ptr = std::is_same<Traits, v8pp::shared_ptr_traits>::value;

	check_eq("Y count after GC", Y::instance_count,
		1 + 2 * use_shared_ptr); // y1 + (y2 + y3 when use_shared_ptr)

	v8pp::class_<Y, Traits>::destroy(isolate);
	check_eq("Y count after destroy", Y::instance_count,
		1 + 2 * use_shared_ptr); // y1 + (y2 + y3 when use_shared_ptr)

	v8pp::class_<Y, Traits>::destroy(isolate);
	check_eq("Y count after class_<Y>::destroy", Y::instance_count,
		1 + 2 * use_shared_ptr); // y1 + (y2 + y3 when use_shared_ptr)
}

template<typename Traits>
void test_multiple_inheritance()
{
	struct A
	{
		int x;
		A() : x(1) {}
		int f() { return x; }
		void set_f(int v) { x = v; }

		int z() const { return x; }
	};

	struct B
	{
		int x;
		B() : x(2) {}
		int g() { return x; }
		void set_g(int v) { x = v; }

		int z() const { return x; }
	};

	struct C : A, B
	{
		int x;
		C() : x(3) {}
		int h() { return x; }
		void set_h(int v) { x = v; }

		int z() const { return x; }
	};

	v8pp::context context;
	v8::Isolate* isolate = context.isolate();
	v8::HandleScope scope(isolate);

	v8pp::class_<B, Traits> B_class(isolate);
	B_class
		.var("xB", &B::x)
		.function("zB", &B::z)
		.function("g", &B::g);

	v8pp::class_<C, Traits> C_class(isolate);
	C_class
		.template inherit<B>()
		.template ctor<>()
		.var("xA", &A::x)
		.var("xC", &C::x)

		.function("zA", &A::z)
		.function("zC", &C::z)

		.function("f", &A::f)
		.function("h", &C::h)

		.property("rF", &C::f)
		.property("rG", &C::g)
		.property("rH", &C::h)

		.property("F", &C::f, &C::set_f)
		.property("G", &C::g, &C::set_g)
		.property("H", &C::h, &C::set_h)
		;


	context.class_("C", C_class);
	check_eq("get attributes", run_script<int>(context, "c = new C(); c.xA + c.xB + c.xC"), 1 + 2 + 3);
	check_eq("set attributes", run_script<int>(context,
		"c = new C(); c.xA = 10; c.xB = 20; c.xC = 30; c.xA + c.xB + c.xC"), 10 + 20 + 30);

	check_eq("functions", run_script<int>(context, "c = new C(); c.f() + c.g() + c.h()"), 1 + 2 + 3);
	check_eq("z functions", run_script<int>(context, "c = new C(); c.zA() + c.zB() + c.zC()"), 1 + 2 + 3);

	check_eq("rproperties", run_script<int>(context,
		"c = new C(); c.rF + c.rG + c.rH"), 1 + 2 + 3);
	check_eq("rwproperties", run_script<int>(context,
		"c = new C(); c.F = 100; c.G = 200; c.H = 300; c.F + c.G + c.H"), 100 + 200 + 300);
}

void test_class()
{
	test_class_<v8pp::raw_ptr_traits>();
	test_class_<v8pp::shared_ptr_traits>();

	test_multiple_inheritance<v8pp::raw_ptr_traits>();
	test_multiple_inheritance<v8pp::shared_ptr_traits>();
}
