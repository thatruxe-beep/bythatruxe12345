#pragma once

class LoaderController;
struct LoaderModel;

// Шрифты (Segoe UI с кириллицей) и стиль Phobia — accent #23CAEE.
bool SetupLoaderFonts(float scale);
void SetupLoaderStyle();

// Один кадр интерфейса загрузчика.
void RenderLoader(LoaderModel& model, LoaderController& controller, float scale);
