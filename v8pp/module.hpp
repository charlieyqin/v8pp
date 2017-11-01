//
// Copyright (c) 2013-2016 Pavel Medvedev. All rights reserved.
//
// This file is part of v8pp (https://github.com/pmed/v8pp) project.
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef V8PP_MODULE_HPP_INCLUDED
#define V8PP_MODULE_HPP_INCLUDED

#include <v8.h>

#include "v8pp/function.hpp"
#include "v8pp/property.hpp"

namespace v8pp {

template<typename T, typename Traits>
class class_;

/// Module (similar to v8::ObjectTemplate)
class module
{
public:
	/// Create new module in the specified V8 isolate
	explicit module(v8::Isolate* isolate)
		: isolate_(isolate)
		, obj_(v8::ObjectTemplate::New(isolate))
	{
	}

	/// Create new module in the specified V8 isolate for existing ObjectTemplate
	explicit module(v8::Isolate* isolate, v8::Handle<v8::ObjectTemplate> obj)
		: isolate_(isolate)
		, obj_(obj)
	{
	}

	/// v8::Isolate where the module belongs
	v8::Isolate* isolate() { return isolate_; }

	/// Set a V8 value in the module with specified name
	template<typename Data>
	module& set_value(char const* name, v8::Handle<Data> value)
	{
		obj_->Set(v8pp::to_v8(isolate_, name), value);
		return *this;
	}

	/// Set submodule in the module with specified name
	module& set_submodule(char const* name, module& m)
	{
		return set_value(name, m.obj_);
	}

	/// Set wrapped C++ class in the module with specified name
	template<typename T, typename Traits>
	module& set_class(char const* name, class_<T, Traits>& cl)
	{
		v8::HandleScope scope(isolate_);

		cl.class_function_template()->SetClassName(v8pp::to_v8(isolate_, name));
		return set_value(name, cl.js_function_template());
	}

	/// Set a C++ function in the module with specified name
	template<typename Function, typename Traits = raw_ptr_traits>
	module& set_function(char const* name, Function&& func)
	{
		using Fun = typename std::decay<Function>::type;
		static_assert(detail::is_callable<Fun>::value, "Function must be callable");
		return set_value(name, wrap_function_template<Traits>(isolate_, std::forward<Function>(func)));
	}

	/// Set a C++ variable in the module with specified name
	template<typename Variable>
	module& set_var(char const *name, Variable& var)
	{
		static_assert(!detail::is_callable<Variable>::value, "Variable must not be callable");
		v8::HandleScope scope(isolate_);

		obj_->SetAccessor(v8pp::to_v8(isolate_, name),
			&var_get<Variable>, &var_set<Variable>,
			detail::set_external_data(isolate_, &var),
			v8::DEFAULT, v8::PropertyAttribute(v8::DontDelete));
		return *this;
	}

	/// Set property in the module with specified name and get/set functions
	template<typename GetFunction, typename SetFunction>
	module& set_property(char const *name, GetFunction&& get, SetFunction&& set)
	{
		using Getter = typename std::decay<GetFunction>::type;
		using Setter = typename std::decay<SetFunction>::type;
		static_assert(detail::is_callable<Getter>::value, "GetFunction must be callable");
		static_assert(detail::is_callable<Setter>::value, "SetFunction must be callable");

		using property_type = property_<Getter, Setter>;

		v8::HandleScope scope(isolate_);

		obj_->SetAccessor(v8pp::to_v8(isolate_, name),
			property_type::get, property_type::set,
			detail::set_external_data(isolate_, property_type(get, set)),
			v8::DEFAULT, v8::PropertyAttribute(v8::DontDelete));
		return *this;
	}

	/// Set read-only property in the module with specified name and get function
	template<typename GetFunction>
	module& set_property(char const *name, GetFunction&& get)
	{
		using Getter = typename std::decay<GetFunction>::type;
		static_assert(detail::is_callable<Getter>::value, "GetFunction must be callable");

		using property_type = property_<Getter, Getter>;

		v8::HandleScope scope(isolate_);

		obj_->SetAccessor(v8pp::to_v8(isolate_, name),
			property_type::get, nullptr,
			detail::set_external_data(isolate_, property_type(get)),
			v8::DEFAULT, v8::PropertyAttribute(v8::DontDelete | v8::ReadOnly));
		return *this;
	}

	/// Set another module as a read-only property
	module& set_const(char const* name, module& m)
	{
		v8::HandleScope scope(isolate_);

		obj_->Set(v8pp::to_v8(isolate_, name), m.obj_,
			v8::PropertyAttribute(v8::ReadOnly | v8::DontDelete));
		return *this;
	}

	/// Set a value convertible to JavaScript as a read-only property
	template<typename Value>
	module& set_const(char const* name, Value const& value)
	{
		v8::HandleScope scope(isolate_);

		obj_->Set(v8pp::to_v8(isolate_, name), to_v8(isolate_, value),
			v8::PropertyAttribute(v8::ReadOnly | v8::DontDelete));
		return *this;
	}

	/// Create a new module instance in V8
	v8::Local<v8::Object> new_instance() { return obj_->NewInstance(); }

private:
	template<typename Variable>
	static void var_get(v8::Local<v8::String>,
		v8::PropertyCallbackInfo<v8::Value> const& info)
	{
		v8::Isolate* isolate = info.GetIsolate();

		Variable* var = detail::get_external_data<Variable*>(info.Data());
		info.GetReturnValue().Set(to_v8(isolate, *var));
	}

	template<typename Variable>
	static void var_set(v8::Local<v8::String>, v8::Local<v8::Value> value,
		v8::PropertyCallbackInfo<void> const& info)
	{
		v8::Isolate* isolate = info.GetIsolate();

		Variable* var = detail::get_external_data<Variable*>(info.Data());
		*var = v8pp::from_v8<Variable>(isolate, value);
	}

	v8::Isolate* isolate_;
	v8::Handle<v8::ObjectTemplate> obj_;
};

} // namespace v8pp

#endif // V8PP_MODULE_HPP_INCLUDED
