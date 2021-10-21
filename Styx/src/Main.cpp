#include "Layers/ViewerLayer.h"
#include "Charon/Core/Application.h"

using namespace Charon;

int main()
{
	Application application = Application("Vulkan Playground");

	Ref<ViewerLayer> layer = CreateRef<ViewerLayer>();
	application.AddLayer(layer);

	application.Run();

	return 0;
}