/* Copyright © 2001-2014, Canal TP and/or its affiliates. All rights reserved.
  
This file is part of Navitia,
    the software to build cool stuff with public transport.
 
Hope you'll enjoy and contribute to this project,
    powered by Canal TP (www.canaltp.fr).
Help us simplify mobility and open public transport:
    a non ending quest to the responsive locomotion way of traveling!
  
LICENCE: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
   
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Affero General Public License for more details.
   
You should have received a copy of the GNU Affero General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
  
Stay tuned using
twitter @navitia 
IRC #navitia on freenode
https://groups.google.com/d/forum/navitia
www.navitia.io
*/

#include "ed/data.h"
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE test_ed
#include <boost/test/unit_test.hpp>
#include <boost/range/algorithm.hpp>

namespace std {
static std::ostream& operator<<(std::ostream& os, const navitia::type::LineString& ls) {
    os << "[";
    auto it = ls.begin(), end = ls.end();
    if (it != end) os << *it++;
    for (; it != end; ++it) os << ", " << *it;
    return os << "]";
}
}

//     + A
// M +
//   |
//   |          D
//   |   B C     +
//   |   + +       + E
// N +---------+
//           O |
//             |
//             |
//             |   + F
//           P +
// 0.001 ~= 100m
static const navitia::type::GeographicalCoord M(0.010,0.020);
static const navitia::type::GeographicalCoord N(0.010,0.015);
static const navitia::type::GeographicalCoord O(0.015,0.015);
static const navitia::type::GeographicalCoord P(0.015,0.010);
static const navitia::type::GeographicalCoord A(0.011,0.021);
static const navitia::type::GeographicalCoord B(0.012,0.016);
static const navitia::type::GeographicalCoord C(0.013,0.016);
static const navitia::type::GeographicalCoord D(0.016,0.017);
static const navitia::type::GeographicalCoord E(0.017,0.016);
static const navitia::type::GeographicalCoord F(0.017,0.011);
static const navitia::type::LineString shape = {M, N, O, P};
static const navitia::type::LineString rshape = {P, O, N, M};

static void shape_eq(const navitia::type::GeographicalCoord& from,
                     const navitia::type::GeographicalCoord& to,
                     const navitia::type::LineString expected) {
    using ed::create_shape;

    navitia::type::LineString rexpected = expected;
    boost::reverse(rexpected);

    BOOST_REQUIRE_EQUAL(create_shape(from, to, shape), expected);
    BOOST_REQUIRE_EQUAL(create_shape(from, to, rshape), expected);
    BOOST_REQUIRE_EQUAL(create_shape(to, from, shape), rexpected);
    BOOST_REQUIRE_EQUAL(create_shape(to, from, rshape), rexpected);
}

BOOST_AUTO_TEST_CASE(create_shape) {
    typedef navitia::type::LineString LS;

    // nearest on same segment: direct path
    shape_eq(B, C, LS({B, C}));

    // nearest on same point: direct path
    shape_eq(D, E, LS({D, E}));

    // nearest segment and point on segment: using the common point
    shape_eq(B, D, LS({B, O, D}));

    // 2 nearest segments are adjacent: using the common point
    shape_eq(B, F, LS({B, O, F}));

    // A->F, a long path
    shape_eq(A, F, LS({A, M, N, O, F}));
}
