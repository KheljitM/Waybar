#include "modules/temperature.hpp"

#include <filesystem>

waybar::modules::Temperature::Temperature(const std::string& id, const Json::Value& config)
    : ALabel(config, "temperature", id, "{temperatureC}°C", 10) {
  if (config_["hwmon-path"].isString()) {
    file_path_ = config_["hwmon-path"].asString();
  } else if (config_["hwmon-path-abs"].isString() && config_["input-filename"].isString()) {
    file_path_ = (*std::filesystem::directory_iterator(config_["hwmon-path-abs"].asString()))
                     .path()
                     .string() +
                 "/" + config_["input-filename"].asString();
  } else {
    auto zone = config_["thermal-zone"].isInt() ? config_["thermal-zone"].asInt() : 0;
    file_path_ = fmt::format("/sys/class/thermal/thermal_zone{}/temp", zone);
  }
  std::ifstream temp(file_path_);
  if (!temp.is_open()) {
    throw std::runtime_error("Can't open " + file_path_);
  }
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

auto waybar::modules::Temperature::update() -> void {
  auto temperature = getTemperature();
  uint16_t temperature_c = std::round(temperature);
  uint16_t temperature_f = std::round(temperature * 1.8 + 32);
  uint16_t temperature_k = std::round(temperature + 273.15);
  auto critical = isCritical(temperature_c);
  auto format = format_;
  if (critical) {
    format = config_["format-critical"].isString() ? config_["format-critical"].asString() : format;
    label_.get_style_context()->add_class("critical");
  } else {
    label_.get_style_context()->remove_class("critical");
  }

  if (format.empty()) {
    event_box_.hide();
    return;
  } else {
    event_box_.show();
  }

  auto max_temp = config_["critical-threshold"].isInt() ? config_["critical-threshold"].asInt() : 0;
  label_.set_markup(fmt::format(format, fmt::arg("temperatureC", temperature_c),
                                fmt::arg("temperatureF", temperature_f),
                                fmt::arg("temperatureK", temperature_k),
                                fmt::arg("icon", getIcon(temperature_c, "", max_temp))));
  if (tooltipEnabled()) {
    std::string tooltip_format = "{temperatureC}°C";
    if (config_["tooltip-format"].isString()) {
      tooltip_format = config_["tooltip-format"].asString();
    }
    label_.set_tooltip_text(fmt::format(tooltip_format, fmt::arg("temperatureC", temperature_c),
                                        fmt::arg("temperatureF", temperature_f),
                                        fmt::arg("temperatureK", temperature_k)));
  }
  // Call parent update
  ALabel::update();
}

float waybar::modules::Temperature::getTemperature() {
  std::ifstream temp(file_path_);
  if (!temp.is_open()) {
    throw std::runtime_error("Can't open " + file_path_);
  }
  std::string line;
  if (temp.good()) {
    getline(temp, line);
  }
  temp.close();
  auto temperature_c = std::strtol(line.c_str(), nullptr, 10) / 1000.0;
  return temperature_c;
}

bool waybar::modules::Temperature::isCritical(uint16_t temperature_c) {
  return config_["critical-threshold"].isInt() &&
         temperature_c >= config_["critical-threshold"].asInt();
}
