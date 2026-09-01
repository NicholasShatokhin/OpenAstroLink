from pathlib import Path
root=Path(__file__).resolve().parents[1]

def text(path): return (root/path).read_text(encoding='utf-8')
def need(path,*tokens):
    s=text(path)
    missing=[t for t in tokens if t not in s]
    if missing: raise SystemExit(f'{path}: missing {missing}')

need('CMakeLists.txt','VERSION 0.2.10.47')
need('src/core/astro_types.h',
     'struct ObservationPlan','struct ObservationBlock','enum class ObservationMode { DsoFits, PlanetarySer }',
     'struct DsoFitsBlock','struct PlanetarySerBlock','observationPlanToJson','observationPlanFromJson',
     'currentBlockCompletedFrames','currentStep','currentOperationId','lastError')
need('src/algorithms/scheduler.h','void setPlan(ObservationPlan plan)','void setLegacyPlan','void markFrameCompleted()','void advanceBlock()')
need('src/algorithms/scheduler.cpp','status_.currentStep = "prepare-block"','status_.state = "completed"','b.dso.exposure.saveRaw = true')
need('src/core/observatory_controller.h','setObservationPlan(const ObservationPlan &plan')
need('src/core/application_controller.cpp',
     'startCurrentDsoSlew','startCurrentDsoSolve','startCurrentDsoAutofocus','startCurrentDsoCapture',
     'startMountSlew(block->coordinate','startAdaptiveSolve(r','startAutofocus(block->dso.autofocus.request','startCapture(r',
     'Plate-solve Sync failed before recenter','syncMount(solvedJ2000','Recenter did not converge',
     'r.saveRaw=true','everyNFrames','startCurrentPlanetarySer')
need('src/core/remote_observatory_controller.cpp','setObservationPlan(const ObservationPlan&','observationPlanToJson(pendingPlan_)','currentBlockCompletedFrames','currentOperationId')
need('src/oal/oal_server.cpp','b.contains("blocks")','observationPlanFromJson(b)','/api/v1/sessions/current/plan','setSessionPlan')
need('src/gui/main_window.cpp','Add DSO block','setObservationPlan(p','Recenter every N frames','Autofocus every N frames')
need('docs/SCHEDULER.md','v0.2.10.47','Sync + correction slew','planetary-ser')
need('docs/uk/SCHEDULER.md','v0.2.10.47','Sync + correction slew','planetary-ser')
need('docs/openapi.yaml','version: 0.2.10.47','ObservationBlock:','DsoFitsBlock:','/sessions/current/plan:')
print('scheduler ObservationPlan + retained DSO executor v0.2.10.47: PASS')
