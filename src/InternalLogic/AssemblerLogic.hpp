#pragma once
#include <iostream>

#define Add associate

template <typename LeftType, typename RightType>
LeftType &associate(LeftType &left, RightType &right)
{
    left.associate(right);
    return left;
}