#include "Layers/ViewerLayer.h"
#include "Layers/ParticleLayer.h"
#include "Charon/Core/Application.h"

using namespace Charon;

int main()
{
	Application application = Application("Vulkan Playground");

	Ref<ParticleLayer> layer = CreateRef<ParticleLayer>();
	application.AddLayer(layer);

	application.Run();

	return 0;
}