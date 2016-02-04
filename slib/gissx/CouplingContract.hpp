#pragma once

#include <ostream>
#include <limits>
#include <giss/cfnames.hpp>
#include <giss/DynamicEnum.hpp>
#include <giss/VarMetaData.hpp>
#include <giss/exit.hpp>

namespace giss {

struct CoupledField : public giss::VarMetaData {

	std::string name;
	double default_value;
	std::string units;			//!< UDUnits-compatible string
	unsigned flags;			//!< Allows arbitrary subsets
	std::string description;

	// Implementing giss::VarMetaData
	std::string const &get_name() const { return name; }
	std::string const &get_units() const { return units; }
	unsigned get_flags() const { return flags; }
	std::string const &get_description() const { return description; }

	CoupledField(std::string const &_name,
		double _default_value,
		std::string const &_units,
		unsigned _flags,
		std::string const &_description)
	: name(_name), default_value(_default_value),
	units(_units),
	flags(_flags),
	description(_description)
	{}

};

inline std::ostream &operator<<(std::ostream &out, CoupledField const &cf)
	{ return out << "(" << cf.name << ": [" << cf.units << "] flags:" << cf.flags << ")"; } 




struct CouplingContract : public giss::DynamicEnum
{
	std::vector<CoupledField> _ix_to_field;
	std::map<std::string, int> _name_to_ix;
	long _size_nounit;		//!< Num names, not including "unit"
	int _unit_ix;

public:
	CouplingContract() : _size_nounit(0), _unit_ix(1) {}

	auto begin() const -> decltype(_ix_to_field.begin())
		{ return _ix_to_field.begin(); }
	auto end() const -> decltype(_ix_to_field.end())
		{ return _ix_to_field.end(); }

	int add_field(CoupledField &&cf);

	int add_field(CoupledField const &cf) {
		CoupledField dup(cf);
		return add_field(std::move(cf));
	}

	int add_field(std::string const &name, double default_value, std::string const &units,
		unsigned flags = 0,
		std::string const &description = "<no description>")
	{ return add_field(CoupledField(name, default_value, units, flags, description)); }

	long size_withunit() const { return _ix_to_field.size(); }
//	long size() const { return size_withunit(); }
	long size_nounit() const { return _size_nounit; }

	int unit_ix() const { return _unit_ix; }

	int index(std::string const &name, bool throw_exception=true) const {
		auto ii = _name_to_ix.find(name);
		if (ii == _name_to_ix.end()) {
			if (throw_exception) {
				fprintf(stderr, "CouplingContract::index(): name '%s' not found\n", name.c_str());
				giss::exit(1);
			} else return -1;
		}
		return ii->second;
	}

	std::string const &name(int ix) const
		{ return _ix_to_field[ix].name; }

	CoupledField const &field(int ix) const
		{ return _ix_to_field[ix]; }

	CoupledField const &field(std::string const &name) const
		{ return field((*this).index(name)); }

	std::ostream &operator<<(std::ostream &out) const;
 
	friend std::ostream &operator<<(std::ostream &out, CouplingContract const &con);

};

extern std::ostream &operator<<(std::ostream &out, CouplingContract const &con);


}