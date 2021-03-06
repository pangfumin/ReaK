
/*
 *    Copyright 2011 Sven Mikael Persson
 *
 *    THIS SOFTWARE IS DISTRIBUTED UNDER THE TERMS OF THE GNU GENERAL PUBLIC LICENSE v3 (GPLv3).
 *
 *    This file is part of ReaK.
 *
 *    ReaK is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    ReaK is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with ReaK (as LICENSE in the root folder).  
 *    If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FINITE_DIFFERENCES_HPP
#define FINITE_DIFFERENCES_HPP

#include "function_type.hpp"

namespace ReaK {

namespace optim {

template <class FuncPtr, class Input, class Output, int PointCount = 2>
class CentralFiniteGradient {
  public:
    typedef Output::gradient_by<Input>::result_type result_type;
  private:
    FuncPtr func;
  public:
    result_type computeGradient(const Input& aInput);
};





};

};

#endif









