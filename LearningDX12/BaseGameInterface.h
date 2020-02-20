#pragma once

namespace Olex
{
    struct UpdateEventArgs
    {
        double m_elapsedTime = 0;
        double m_totalTime = 0;
    };

    struct RenderEventArgs
    {
    };

    class BaseGameInterface
    {
    public:
        virtual ~BaseGameInterface() = default;

        virtual void LoadResources() = 0;
        virtual void UnloadResources() = 0;

        virtual void Update(UpdateEventArgs args) = 0;
        virtual void Render(RenderEventArgs args) = 0;
    };
}
