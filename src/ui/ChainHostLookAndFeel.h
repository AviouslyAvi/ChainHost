#pragma once
// ChainHostLookAndFeel — extends Gin's CopperLookAndFeel with our colour scheme
#include <gin_plugin/gin_plugin.h>

class ChainHostLookAndFeel : public gin::CopperLookAndFeel
{
public:
    ChainHostLookAndFeel();
};
