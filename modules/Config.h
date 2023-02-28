/**
 * Config.h
 * Utils for reading/writing properties to a config file
*/

#pragma once

static void writeConfigFile(const String &property, int value)
{
    File config{File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + "/Arboreal Audio/Gamma/config.xml"};
    if (!config.existsAsFile())
        config.create();

    auto xml = parseXML(config);
    if (xml == nullptr)
    {
        xml.reset(new XmlElement("Config"));
    }

    if (property == "size" && value > 1600)
        value = 1600;

    xml->setAttribute(property, value);
    xml->writeTo(config);
}

static void writeConfigFileString(const String &property, const String &value)
{
    File config{File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + "/Arboreal Audio/Gamma/config.xml"};
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

/*returns integer value of read property, or -1 if it doesn't exist*/
static int readConfigFile(const String &property)
{
    File config{File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + "/Arboreal Audio/Gamma/config.xml"};
    if (!config.existsAsFile())
        return -1;

    auto xml = parseXML(config);
    if (xml != nullptr && xml->hasTagName("Config"))
    {
        return xml->getIntAttribute(property, -1);
    }

    return -1;
}

static String readConfigFileString(const String &property)
{
    File config{File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + "/Arboreal Audio/Gamma/config.xml"};
    if (!config.existsAsFile())
        return "";

    auto xml = parseXML(config);
    if (xml != nullptr && xml->hasTagName("Config"))
    {
        return xml->getStringAttribute(property);
    }

    return "";
}