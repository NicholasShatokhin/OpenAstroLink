#pragma once

#include "core/interfaces.h"
#include <QString>

namespace oas {

// Production plate-solver adapter for ASTAP/astap_cli.
//
// The adapter deliberately talks to ASTAP through its documented command-line
// interface.  This keeps ASTAP out of the OAL process and makes the same code
// usable on Windows and on the Raspberry Pi observatory node.
class AstapSolver final : public IPlateSolver {
public:
    explicit AstapSolver(QString executable = {}, QString databasePath = {});

    QString name() const override { return "ASTAP"; }
    SolveResult solve(const CameraFrame &frame, const TelescopeProfile &profile,
                      const SolveHint &hint = {}) override;

    QString executable() const { return executable_; }
    QString databasePath() const { return databasePath_; }
    bool available(QString *reason = nullptr) const;

    static QString findExecutable();

private:
    QString executable_;
    QString databasePath_;
};

} // namespace oas
