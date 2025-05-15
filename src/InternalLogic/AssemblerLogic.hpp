#pragma once
#include <iostream>

#define Add add

template <typename LeftType, typename RightType>
LeftType &add(LeftType &left, RightType &right)
{
    left.add(right);
    return left;
    std::cout << "[DEBUG] add() called" << std::endl;
}
