#ifndef FILE_RECIPE_H
#define FILE_RECIPE_H

#include <vector>
#include <string>
#include <stdint.h>

void saveFileRecipe(std::vector<std::string> file_recipe, const char* fileRecipesPath);
std::string getRecipeNameFromVersion(uint8_t restore_version, const char* file_recipe_path);
bool fileRecipeExist(uint8_t restore_version, const char* file_recipe_path);
std::vector<std::string> getFileRecipe(uint8_t restore_version, const char* file_recipe_path);

#endif // FILE_RECIPE_H
