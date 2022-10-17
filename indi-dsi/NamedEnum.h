/*
 * Copyright © 2008, Roland Roberts
 *
 */

#pragma once

#include <algorithm>
#include <functional>
#include <string>
#include <set>
namespace DSI
{
    /* */
template <class TValue, class T>
class NamedEnum
{
  protected:
    // Constructors
    NamedEnum(const std::string _name, const TValue &_value);

  private:
    // Predicate for finding the corresponding instance
    struct Enum_Predicate_Corresponds
    {
        typedef const NamedEnum<TValue, T> * argument_type;
        typedef bool result_type;

        Enum_Predicate_Corresponds(const TValue &Value) : m_value(Value) {}
        bool operator()(const NamedEnum<TValue, T> *E) const { return E->value() == m_value; }

      private:
        const TValue &m_value;
    };

    // Comparison functor for the set of instances
    struct NamedEnum_Ptr_LessThan
    {
        typedef const NamedEnum<TValue, T> * first_argument_type;
        typedef const NamedEnum<TValue, T> * second_argument_type;
        typedef bool result_type;

        bool operator()(const NamedEnum<TValue, T> *E_1, const NamedEnum<TValue, T> *E_2) const
        {
            return E_1->value() < E_2->value();
        }
    };

  public:
    // Compiler-generated copy constructor and operator= are OK.

    typedef std::set<const NamedEnum<TValue, T> *, NamedEnum_Ptr_LessThan> instances_list;
    typedef typename instances_list::const_iterator const_iterator;

    bool operator==(const TValue _value) const { return this->value() == _value; }
    bool operator!=(const TValue _value) const { return !(this->value() == _value); }
    bool operator==(const T &d) const { return this->value() == d.value(); }

    // Access to TValue value
    const TValue &value(void) const { return m_value; }

    const std::string &name(void) const { return m_name; }

    static const TValue &min(void) { return (*getInstances().begin())->m_value; }

    static const TValue &max(void) { return (*getInstances().rbegin())->m_value; }

    static const T *find(const TValue &Value)
    {
        const_iterator it = find_if(begin(), end(), Enum_Predicate_Corresponds(Value));
        return (it != end()) ? (T *)*it : NULL;
    }

    static bool isValidValue(const TValue &Value) { return find(Value) != NULL; }

    bool operator==(const NamedEnum<TValue, T> &x) const { return (this == &x); }

    bool operator!=(const NamedEnum<TValue, T> &x) const { return !(this == &x); }

    // Number of elements
    static typename instances_list::size_type size(void) { return getInstances().size(); }

    // Iteration
    static const_iterator begin(void) { return getInstances().begin(); }
    static const_iterator end(void) { return getInstances().end(); }

  private:
    static instances_list &getInstances()
    {
      static instances_list instances;
      return instances;
    }

  private:
    TValue m_value;
    std::string m_name;
    //static instances_list s_instances;
};

template <class TValue, class T>
inline NamedEnum<TValue, T>::NamedEnum(std::string name, const TValue &value) : m_value(value), m_name(name)
{
    getInstances().insert(this);
}
}
