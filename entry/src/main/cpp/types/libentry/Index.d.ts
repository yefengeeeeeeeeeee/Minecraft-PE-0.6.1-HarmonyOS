// NAPI 接口声明（entry/src/main/cpp/main_ohos.cpp 的导出）
// 说明：本工程没有把 libentry 作为 oh-package 声明类型，
// 这里主要给 IDE 做提示用，签名需与 main_ohos.cpp 的 Init() 保持一致。

export const initRender: (resMgr: object, surfaceId: string, width: number, height: number, filesDir: string) => number;
export const renderFrame: () => void;
export const renderTouch: (x: number, y: number, action: number, pointerId: number) => void;
export const handleKeyBoard: (action: number, keyCode: number) => void;
export const handleKeyText: (text: string, keyCode: number) => void;
export const feedText: (text: string) => void;
export const isKeyDown: (keyCode: number) => boolean;
export const isKeyboardRequested: () => boolean;
export const destroyRender: () => void;
