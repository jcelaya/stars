/*
 *  PeerComp - Highly Scalable Distributed Computing Architecture
 *  Copyright (C) 2007 Javier Celaya
 *
 *  This file is part of PeerComp.
 *
 *  PeerComp is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  PeerComp is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with PeerComp; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <sstream>
#include <fstream>
#include <iomanip>
#include <map>
#include <list>
using namespace std;


static void subst(const map<string, string> & p, string & value, size_t substStart = 0) {
	if ((substStart = value.find("${", substStart)) != string::npos) {
		subst(p, value, substStart + 2);
		string keyn = value.substr(substStart + 2, value.find_first_of('}', substStart + 2) - substStart - 2);
		// Check if we have to do a vector substitution
		string substStr;
		size_t firstpos;
		if ((firstpos = keyn.find_first_of('=')) != string::npos) {
			map<string, string>::const_iterator it = p.find(keyn.substr(0, firstpos));
			if (it != p.end()) {
				int i;
				if ((istringstream(it->second) >> i).eof()) {
					for (size_t lastpos = firstpos; lastpos != string::npos && i >= 0; i--) {
						firstpos = lastpos + 1;
						lastpos = keyn.find_first_of('|', firstpos);
						substStr = keyn.substr(firstpos, lastpos - firstpos);
					}
				}
			}
		} else {
			map<string, string>::const_iterator it = p.find(keyn);
			if (it != p.end()) {
				substStr = it->second;
			}
		}
		value.replace(substStart, keyn.size() + 3, substStr);
	}
}


static bool getInterval(const map<string, string> & p, const string & v, char delim, double & startValue, double & stepValue, double & endValue) {
	size_t firstDash, secondDash;
	// Check if this value is an interval
	firstDash = v.find_first_of(delim);
	secondDash = v.find_first_of(delim, firstDash + 1);
	if (secondDash != string::npos && v.find_first_of(delim, secondDash + 1) == string::npos) {
		// It is an interval
		string firstVal = v.substr(0, firstDash),
				secondVal = v.substr(firstDash + 1, secondDash - firstDash - 1),
				thirdVal = v.substr(secondDash + 1);
		subst(p, firstVal);
		subst(p, secondVal);
		subst(p, thirdVal);
		// Evaluate interval values and check whether they are numbers
		if ((istringstream(firstVal) >> startValue).eof() &&
			(istringstream(secondVal) >> stepValue).eof() &&
			(istringstream(thirdVal) >> endValue).eof()) {
			// All three where just numbers
			return true;
		}
	}
	return false;
}


struct Section {
	list<pair<string, list<string> > > values;
	map<string, list<pair<string, list<string> > >::iterator> lookup;

	void add(const string & key) {
		lookup[key] = values.insert(values.end(), make_pair(key, list<string>()));
	}

	list<string> & operator[](const string & key) {
		if (!lookup.count(key)) add(key);
		return lookup[key]->second;
	}

	Section & operator=(const Section & copy) {
		values = copy.values;
		// Reconstruct lookup table
		for (list<pair<string, list<string> > >::iterator it = values.begin(); it != values.end(); it++)
			lookup[it->first] = it;
		return *this;
	}
};


void getPropertiesList(const string & fileName, std::list<std::map<std::string, std::string> > & combinations) {
	ifstream ifs(fileName.c_str());
	string nextLine;
	unsigned int sectionNumber = 1;

	// Skip until start of section or end of file
	while (ifs.good()) {
		getline(ifs, nextLine);
		if (nextLine[0] == '[') break;
	}

	map<string, Section> sections;
	list<string> sectionOrder;

	while (ifs.good()) {
		// Get section name.
		// It is in the form casename:name:supername, spaces allowed
		string sectionName, caseName, superName;
		size_t first, second;
		first = nextLine.find_first_of(':');
		if (first != string::npos) {
			caseName = nextLine.substr(1, first - 1);
			second = nextLine.find_first_of(':', first + 1);
			if (second != string::npos) {
				sectionName = nextLine.substr(first + 1, second - first - 1);
				superName = nextLine.substr(second + 1, nextLine.find_first_of(']') - second - 1);
			} else
				sectionName = nextLine.substr(first + 1, nextLine.find_first_of(']') - first - 1);
		} else
			caseName = nextLine.substr(1, nextLine.find_first_of(']') - 1);

		// Default name
		if (sectionName == "") {
			ostringstream oss;
			oss << "unnamed" << sectionNumber++;
			sectionName = oss.str();
		}

		if (sections.count(sectionName)) {
			// Skip this section as a previous one already exists with the same name.
			while (ifs.good()) {
				// Read next line
				getline(ifs, nextLine);
				if (nextLine[0] == '[') break;
			}
			continue;
		}
		Section & section = sections[sectionName];
		sectionOrder.push_back(sectionName);

		// Inherit from super section
		if (sections.count(superName)) {
			section = sections[superName];
			section["case_name"].clear();
			section["section_name"].clear();
			section["super_name"].clear();
		}
		section["case_name"].push_back(caseName);
		section["section_name"].push_back(sectionName);
		section["super_name"].push_back(superName);

		while (ifs.good()) {
			// Read next line
			getline(ifs, nextLine);
			// If it is just an empty line, skip it
			if (nextLine.length() == 0) continue;
			// If it is a comment, skip it
			if (nextLine[0] == '#') continue;
			// If it is the name of a section, exit
			if (nextLine[0] == '[') break;

			// Get key name, no spaces allowed
			size_t firstpos, lastpos = nextLine.find_first_of('=');
			if (lastpos == string::npos) continue; // wrong format
			string key;
			istringstream(nextLine.substr(0, lastpos)) >> skipws >> key;
			// Values are "value,interval,..."
			// BEWARE!! Spaces are not ignored, as they can be part of valid values
			list<string> & values = section[key];
			values.clear();
			while (lastpos != string::npos) {
				firstpos = lastpos + 1;
				lastpos = nextLine.find_first_of(',', firstpos);
				values.push_back(nextLine.substr(firstpos, lastpos - firstpos));
			}
		}

	}

	// Create combinations
	for (list<string>::iterator it = sectionOrder.begin(); it != sectionOrder.end(); it++) {
		Section & section = sections[*it];
		// Get all the combinations of properties for that test
		list<map<string, string> > cc;
		cc.push_back(map<string, string>());

		for (list<pair<string, list<string> > >::iterator keyvalue = section.values.begin(); keyvalue != section.values.end(); keyvalue++) {
			const string & key = keyvalue->first;
			list<string> & values = keyvalue->second;
			list<map<string, string> > newProperties;
			for (list<map<string, string> >::iterator p = cc.begin(); p != cc.end(); p++) {
				for (list<string>::iterator value = values.begin(); value != values.end(); value++) {
					double startValue, stepValue, endValue;
					if (getInterval(*p, *value, '-', startValue, stepValue, endValue)) {
						// It is an arithmetic progression
						for (;startValue <= endValue; startValue += stepValue) {
							newProperties.push_back(*p);
							ostringstream oss;
							oss << setprecision(100) << startValue;
							newProperties.back()[key] = oss.str();
						}
					} else if (getInterval(*p, *value, '^', startValue, stepValue, endValue)) {
						// It is a geometric progression
						for (;startValue <= endValue; startValue *= stepValue) {
							newProperties.push_back(*p);
							ostringstream oss;
							oss << setprecision(100) << startValue;
							newProperties.back()[key] = oss.str();
						}
					} else {
						// It is a normal value
						newProperties.push_back(*p);
						newProperties.back()[key] = *value;
						subst(*p, newProperties.back()[key]);
					}
				}
			}
			cc.swap(newProperties);
		}

		combinations.splice(combinations.end(), cc);
	}
}
