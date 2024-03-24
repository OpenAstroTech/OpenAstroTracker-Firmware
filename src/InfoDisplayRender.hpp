#pragma once

class Mount;

class InfoDisplayRender
{
  public:
    InfoDisplayRender() {};

    virtual void init() {};
    virtual void render(Mount *mount) {};
};

