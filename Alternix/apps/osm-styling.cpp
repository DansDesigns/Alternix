#include <fstream>
#include <iostream>
#include <QApplication>
#include <QScreen>
#include <cstdlib>

// Function to generate a basic GTK stylesheet
void generateGTKStylesheet(const std::string& filename, int fontSize) {
    std::ofstream file(filename);
    if (file.is_open()) {
        file << "* {" << std::endl;
        file << "  background-color: rgb(40, 40, 40);" << std::endl;
        file << "  color: white;" << std::endl;
        file << "  font-size: " << fontSize << "px;" << std::endl;
        file << "}" << std::endl;
        file.close();
    }
}

// Function to generate a basic Qt stylesheet
void generateQtStylesheet(const std::string& filename, int fontSize) {
    std::ofstream file(filename);
    if (file.is_open()) {
        file << "QWidget {" << std::endl;
        file << "  background-color: rgb(40, 40, 40);" << std::endl;
        file << "  color: white;" << std::endl;
        file << "  font-size: " << fontSize << "px;" << std::endl;
        file << "}" << std::endl;
        file.close();
    }
}

// Function to apply the GTK stylesheet (for example, by copying it to the GTK config folder)
void applyGTKStylesheet(const std::string& filename) {
    std::system(("cp " + filename + " ~/.config/gtk-3.0/gtk.css").c_str());
}

// Function to apply the Qt stylesheet (by setting the QT_STYLESHEETS environment variable or similar)
void applyQtStylesheet(const std::string& filename) {
    std::system(("export QT_STYLE_SHEETS=" + filename).c_str());
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // Get the primary screen size
    QScreen *screen = app.primaryScreen();
    int screenWidth = screen->size().width();
    
    // Decide on font size based on screen width
    int fontSize = (screenWidth < 780) ? 24 : 20;

    // Generate the stylesheets with the chosen font size
    generateGTKStylesheet("alternix-theme.css", fontSize);
    generateQtStylesheet("alternix-theme.qss", fontSize);
    
    // Apply the stylesheets to the system
    applyGTKStylesheet("alternix-theme.css");
    applyQtStylesheet("alternix-theme.qss");
    
    return 0;
}

