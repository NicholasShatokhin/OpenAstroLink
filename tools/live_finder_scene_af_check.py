from pathlib import Path
checks = {
    'CMakeLists.txt': ['VERSION 0.2.10.35'],
    'src/core/astro_types.h': ['AutofocusMode { Stars, Scene, Planet, Bahtinov }', 'struct LiveViewRequest', 'int minStars{3}'],
    'src/core/application_controller.cpp': ['camera.live-view', 'Live View operation accepted', 'commitCapturedFrame(frame,false,false)', 'previewFrameCache_.size()>8', 'Canon live view requires the EDSDK EVF transport'],
    'src/oal/oal_server.cpp': ['/api/v1/cameras/<arg>/live-view', 's=="scene"', 'q.minStars'],
    'src/gui/main_window.cpp': ['Live / Finder', 'Finder Alignment wizard', 'Scene autofocus using these camera settings', 'No robust bright region detected' if False else 'Bright target: no robust bright region detected'],
    'src/algorithms/autofocus_engine.cpp': ['No suitable stars detected', 'Scene autofocus completed', 'r.exposureSec', 'r.gain'],
    'site/index.html': ['openastro.link', 'NicholasShatokhin/OpenAstroLink'],
    'site/uk/index.html': ['OpenAstroLink', 'Монтування'],
}
count=0
for file, needles in checks.items():
    text=Path(file).read_text(encoding='utf-8')
    for needle in needles:
        assert needle in text, f'{file}: missing {needle}'
        print(f'PASS: {file}: {needle}')
        count += 1
print(f'live/finder/scene-AF v0.2.10.35: PASS ({count} assertions)')
