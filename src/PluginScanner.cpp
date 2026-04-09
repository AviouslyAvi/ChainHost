#include "PluginScanner.h"

PluginScanner::PluginScanner()
{
    formatManager.addDefaultFormats();
    loadFromCache();
}

PluginScanner::~PluginScanner()
{
    stopTimer();
}

void PluginScanner::scan()
{
    if (isScanning())
        return;

    currentFormatIndex = 0;
    startNextFormat();
}

void PluginScanner::startNextFormat()
{
    scanner.reset();

    while (currentFormatIndex < formatManager.getNumFormats())
    {
        auto* format = formatManager.getFormat (currentFormatIndex);
        auto searchPaths = format->getDefaultLocationsToSearch();

        if (searchPaths.getNumPaths() > 0)
        {
            // Use a deadman file so if a plugin crashes during scan,
            // it gets blacklisted and skipped next time
            auto deadManFile = getCacheFile().getSiblingFile ("scanning_deadman");

            scanner = std::make_unique<juce::PluginDirectoryScanner> (
                knownPlugins,
                *format,
                searchPaths,
                true,       // recursive
                deadManFile
            );

            // Scan one plugin at a time on the message thread timer
            startTimer (1);
            return;
        }

        ++currentFormatIndex;
    }

    // All formats done
    saveToCache();

    if (onScanComplete)
        onScanComplete();
}

void PluginScanner::timerCallback()
{
    if (! scanner)
    {
        stopTimer();
        return;
    }

    juce::String pluginName;

    // Scan a few plugins per timer tick to keep UI responsive
    for (int i = 0; i < 2; ++i)
    {
        if (! scanner->scanNextFile (true, pluginName))
        {
            // This format is done, move to next
            stopTimer();
            ++currentFormatIndex;
            startNextFormat();
            return;
        }
    }
}

juce::File PluginScanner::getCacheFile() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("ChainHost");
    dir.createDirectory();
    return dir.getChildFile ("plugin_cache.xml");
}

void PluginScanner::saveToCache()
{
    if (auto xml = knownPlugins.createXml())
        xml->writeTo (getCacheFile());
}

void PluginScanner::loadFromCache()
{
    auto cacheFile = getCacheFile();
    if (cacheFile.existsAsFile())
    {
        if (auto xml = juce::parseXML (cacheFile))
            knownPlugins.recreateFromXml (*xml);
    }
}
