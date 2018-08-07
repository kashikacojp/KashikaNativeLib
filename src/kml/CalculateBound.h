#pragma once 
#ifndef _KML_CALCULATE_BOUND_H_
#define _KML_CALCULATE_BOUND_H_

#include "Bound.h"
#include "Mesh.h"

#include <memory>

namespace kml
{

	std::shared_ptr<Bound> CalculateBound(const std::shared_ptr<Mesh>& mesh, const glm::mat4& mat = glm::mat4(1.0f));

}

#endif
