/**
 * Config.h
 * Utils for reading/writing properties to a config file
*/

#pragma once

/** @param configPath path RELATIVE to user app data dir */
static void writeConfigFile(const String configPath, const String &property, int value)
{
    File config{File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + configPath};
    if (!config.existsAsFile())
        config.create();

    auto xml = parseXML(config);
    if (xml == nullptr)
    {
        xml.reset(new XmlElement("Config"));
    }

    xml->setAttribute(property, value);
    xml->writeTo(config);
}

/** @param configPath path RELATIVE to user app data dir */
static void writeConfigFileString(const String configPath, const String &property, const String &value)
{
    File config{File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + configPath};
    if (!config.existsAsFile())
        config.create();

    auto xml = parseXML(config);
    if (xml == nullptr)
    {
        xml.reset(new XmlElement("Config"));
    }

    xml->setAttribute(property, value);
    xml->writeTo(config);
}

/** @param configPath path RELATIVE to user app data dir
 * @brief returns integer value of read property, or -1 if it doesn't exist*/
static int readConfigFile(const String configPath, const String &property)
{
    File config{File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + configPath};
    if (!config.existsAsFile())
        return -1;

    auto xml = parseXML(config);
    if (xml != nullptr && xml->hasTagName("Config"))
    {
        return xml->getIntAttribute(property, -1);
    }

    return -1;
}

/** @param configPath path RELATIVE to user app data dir */
static String readConfigFileString(const String configPath, const String &property)
{
    File config{File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + configPath};
    if (!config.existsAsFile())
        return "";

    auto xml = parseXML(config);
    if (xml != nullptr && xml->hasTagName("Config"))
    {
        return xml->getStringAttribute(property);
    }

    return "";
}