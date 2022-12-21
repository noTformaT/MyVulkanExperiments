#pragma once

// Indieces (locations) of Queue Families (if they exist at all)


struct QueueFamilyIndices
{
	int graphicsFamily = -1;			// Locations of Graphics Queue Family

	// Check if queue family is valid
	bool IsValid()
	{
		return graphicsFamily >= 0;
	}

};