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

// pxPhysics.h

#ifndef PX_PHYSICS_H
#define PX_PHYSICS_H

#ifndef finline
#ifdef WIN32
#define finline __forceinline
#else
#define finline __attribute__((always_inline))
#endif
#endif

#include "rtCore.h"
#include "rtObject.h"
#include "rtObjectMacros.h"

class pxPhysics: public rtObject
{
public:
  rtDeclareObject(pxPhysics, rtObject);

  // Basic 2D Physics - Velocity
  rtProperty(vx, vx, setVX, float);
  rtProperty(vy, vy, setVY, float);
  
  // Basic 2D Physics - Acceleration
  rtProperty(ax, ax, setAX, float);
  rtProperty(ay, ay, setAY, float);

  pxPhysics();

  rtError phyCallback(rtFunctionRef& v) const;
  rtError setPhyCallback(const rtFunctionRef& v);

  rtProperty(phyCallback, phyCallback, setPhyCallback, rtFunctionRef);

  inline float vx()        const { return mVx;                }
  rtError vx(float& v)     const {     v = mVx; return RT_OK; }
  virtual rtError setVX(float v) { mVx = v;   return RT_OK;   }

  inline float vy()        const { return mVy;                }
  rtError vy(float& v)     const {     v = mVy; return RT_OK; }
  virtual rtError setVY(float v) { mVy = v;   return RT_OK;   }

  inline float ax()        const { return mAx;                }
  rtError ax(float& v)     const {     v = mAx; return RT_OK; }
  virtual rtError setAX(float v) { mAx = v;   return RT_OK;   }

  inline float ay()        const { return mAy;                }
  rtError ay(float& v)     const {     v = mAy; return RT_OK; }
  virtual rtError setAY(float v) { mAy = v;   return RT_OK;   }

//private:
  float mVx, mVy, mAx, mAy;

  rtFunctionRef mPhyCallback;

}; // CLASS - pxPhysics

typedef rtRef<pxPhysics> pxPhysicsRef;

#endif // PX_PHYSICS_H

