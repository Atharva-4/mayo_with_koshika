#pragma once

#include "commands_api.h"

namespace Mayo {

    class CommandCutting : public Command {
    public:
        enum class CutPlane {
            X,
            Y,
            Z
        };

        CommandCutting(IAppContext* context);

        void execute() override;

        static constexpr std::string_view Name = "cutting";

    private:
        bool m_isRunning = false;
    };

    class CommandMergeSTL : public Command
    {
    public:
        CommandMergeSTL(IAppContext* context);
        void execute() override;

        static constexpr std::string_view Name = "merge.stl";
    };

    // New: Hole filling commands
    class CommandHoleFillingFull : public Command {
    public:
        CommandHoleFillingFull(IAppContext* context);
        void execute() override;
        static constexpr std::string_view Name = "holefilling.full";

    private:
        bool m_isRunning = false;  
    };


    class CommandHoleFillingSelected : public Command {
    public:
        CommandHoleFillingSelected(IAppContext* context);
        void execute() override;
        static constexpr std::string_view Name = "holefilling.selected";

    private:
        bool m_isRunning = false;
    };

    class CommandPointToSurface : public Command {
    public:
        CommandPointToSurface(IAppContext* context);
        void execute() override;
        static constexpr std::string_view Name = "pointtosurface";
    };

    

} // namespace Mayo
