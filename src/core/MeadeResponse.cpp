/**
 * @file MeadeResponse.cpp
 * @brief Meade response value-type implementation.
 */

#include "core/MeadeResponse.hpp"

namespace oat
{
namespace core
{
namespace meade
{

MeadeResponse::MeadeResponse() : _length(0)
{
    _data[0] = '\0';
}

}  // namespace meade
}  // namespace core
}  // namespace oat
