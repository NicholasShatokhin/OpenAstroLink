#include "algorithms/neural_solver.h"
#include <QFileInfo>
namespace oas {bool NeuralSolver::loadModel(const QString&p,QString*e){if(!QFileInfo::exists(p)){if(e)*e="Model file does not exist";return false;}modelPath_=p;return true;}SolveResult NeuralSolver::solve(const CameraFrame&,const TelescopeProfile&,const SolveHint&){SolveResult r;r.message=modelPath_.isEmpty()?"No neural model loaded":"ONNX runtime adapter is intentionally not compiled in this baseline";return r;}}
