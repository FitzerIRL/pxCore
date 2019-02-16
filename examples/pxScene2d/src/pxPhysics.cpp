/*

 pxCore Copyright 2005-2018 John Robinson

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 */

// pxPhysics.cpp

#include "pxPhysics.h"
//#include "pxObject.h"
#include "pxScene2d.h"

using namespace std;


// pxPhysics methods
pxPhysics::pxPhysics(): rtObject(),
          mVx(0), mVy(0), mAx(0), mAy(0), mPhyCallback(NULL)
{
  // ctor
}

rtError pxPhysics::phyCallback(rtFunctionRef& v) const
{
  v = mPhyCallback;
  return RT_OK;
}

rtError pxPhysics::setPhyCallback(const rtFunctionRef& v)
{
  mPhyCallback = v;

  return RT_OK;
}

rtDefineObject(pxPhysics,   rtObject);
rtDefineProperty(pxPhysics, vx);
rtDefineProperty(pxPhysics, vy);
rtDefineProperty(pxPhysics, ax);
rtDefineProperty(pxPhysics, ay);
rtDefineProperty(pxPhysics, phyCallback);

