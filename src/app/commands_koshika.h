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
    };
    // New: Hole filling commands
    class CommandHoleFillingFull : public Command {
    public:
        CommandHoleFillingFull(IAppContext* context);
        void execute() override;
        static constexpr std::string_view Name = "holefilling.full";
    };

    class CommandHoleFillingSelected : public Command {
    public:
        CommandHoleFillingSelected(IAppContext* context);
        void execute() override;
        static constexpr std::string_view Name = "holefilling.selected";
    };

} // namespace Mayo
