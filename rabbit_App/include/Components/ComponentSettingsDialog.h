
#pragma once
#include "Components/ComponentSettingsDialog.h"
#include "qlineedit.h"
#include "qradiobutton.h"
#include "qtmetamacros.h"
#include "qwidget.h"
#ifndef COMPONENT_SETTINGS_DIALOG_H
#define COMPONENT_SETTINGS_DIALOG_H

#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPushButton>
#include <QRadioButton>
#include <QStandardItemModel>
#include <QTableView>

#include "Components/AbstractComponent.h"
#include "Components/ComponentMacro.h"
#include "Ports/Port.h"
#include "Ports/PortsFileReader.h"

namespace rabbit_App::component {

class AbstractComponent;

/// @brief ComponentSettingsDialog class
/// It is used to display the component settings dialog.
/// It can be used to display the basic settings and ports settings.
/// If you want to add more settings, you can inherit from this class
class ComponentSettingsDialog : public QDialog {
  // Q_OBJECT

  constexpr static int kWindowMinWidth = 480;

public:
  ComponentSettingsDialog(AbstractComponent *component,
                          QWidget *parent = nullptr);
  ~ComponentSettingsDialog();

  const static QMap<QString, QColor> all_supported_color;

protected:
  AbstractComponent *component_;
  bool is_modifieds_ = false;

  /// @brief Add a widget to the basic settings group.
  void appendSettingWidget(QWidget *widget);

  /// @brief Add a layout to the basic settings group.
  void appendSettingLayout(QLayout *layout);

  /// @brief How to apply the derived class settings.
  /// This function is called when the user click the OK button,
  /// in the accept() function.
  virtual void acceptDerivedClassSettings() {}

  /// @brief Accept the dialog when the user click the OK button.
  void accept() override;

private:
  const QList<QString> &getComponentPortsNames();

  void initPortsReader();
  void initUi();
  void initConnections();

  void initTable();

  void addRow(const QVector<ports::Port> &ports, ports::PortType port_type);

  /// @brief Create a combo box with the given ports vec.
  QComboBox *creatCombobox(const QVector<ports::Port> &vec);
  /// @brief Create a model item with the given text.
  QStandardItem *createItem(const QString &text);

  /// @brief Find the HDL port name of the given pin name.
  const QString findHdlPortName(const QVector<ports::Port> &vec,
                                const QString &pin_name);

private:
  /// @brief Basic settings group which contain the basic settings like
  /// name, color, etc.
  QGroupBox *basic_settings_group_;
  /// @brief layout of basic settings group
  QVBoxLayout *basic_settings_layout_;
  /// @brief Name line edit
  QLineEdit *component_name_edit_;

  /// @brief Table view to display the ports.
  QTableView *table_view_;
  /// @brief Model of the table view.
  QStandardItemModel *model_;

  QPushButton *ok_button_;
  QPushButton *cancel_button_;

  /// @brief Original ports names.
  ///       It is used to check if the ports names are modified.
  QList<QString> original_ports_names_;

  /// @brief Ports file reader.
  ///       It is used to read the ports file to get the ports names.
  rabbit_App::ports::PortsFileReader *ports_file_reader_;

}; // class ComponentSettingsDialog

enum class SettingsFeature {
  ActiveMode,
  VisionPersistence,
  Color,
};

template <typename Derived> class SettingsFeatureWidget : public QWidget {
public:
  using QWidget::QWidget;

  void accept(AbstractComponent *component) {
    static_cast<Derived *>(this)->accept(component);
  }
}; // class SettingsFeatureWidget

template <SettingsFeature F> struct WidgetOfFeatureHelper {
  using type = void;
};

template <SettingsFeature F>
using WidgetOfFeature = typename WidgetOfFeatureHelper<F>::type;

class ActiveModeSettingsFeatureWidget
    : public SettingsFeatureWidget<ActiveModeSettingsFeatureWidget> {
public:
  ActiveModeSettingsFeatureWidget(AbstractComponent *component,
                                  QWidget *parent = nullptr);
  void accept(AbstractComponent *component);

private:
  QRadioButton *active_high_radio_button_;
};
template <> struct WidgetOfFeatureHelper<SettingsFeature::ActiveMode> {
  using type = ActiveModeSettingsFeatureWidget;
};

class VisionPersistenceSettingsFeatureWidget
    : public SettingsFeatureWidget<VisionPersistenceSettingsFeatureWidget> {
public:
  VisionPersistenceSettingsFeatureWidget(AbstractComponent *component,
                                         QWidget *parent = nullptr);
  void accept(AbstractComponent *component);

private:
  QLineEdit *vision_persistence_edit_;
};
template <> struct WidgetOfFeatureHelper<SettingsFeature::VisionPersistence> {
  using type = VisionPersistenceSettingsFeatureWidget;
};

class ColorSettingsFeatureWidget
    : public SettingsFeatureWidget<ColorSettingsFeatureWidget> {
public:
  ColorSettingsFeatureWidget(AbstractComponent *component,
                             QWidget *parent = nullptr);
  void accept(AbstractComponent *component);

private:
  QMap<QString, QComboBox *> color_map_;
};
template <> struct WidgetOfFeatureHelper<SettingsFeature::Color> {
  using type = ColorSettingsFeatureWidget;
};

template <SettingsFeature... Features>
class ComponentSettingsDialogWithFeatures : public ComponentSettingsDialog {
  static constexpr auto kFeaturesNum = sizeof...(Features);

public:
  ComponentSettingsDialogWithFeatures(AbstractComponent *component,
                                      QWidget *parent = nullptr)
      : ComponentSettingsDialog(component, parent) {
    initFeatureWidgets();
  }

  void accept() override {
    ComponentSettingsDialog::accept();
    (void)std::initializer_list<int>{(static_cast<WidgetOfFeature<Features> *>(
                                          features_[static_cast<int>(Features)])
                                          ->accept(component_),
                                      0)...};
  }

  void initFeatureWidgets() {
    (void)std::initializer_list<int>{
        (features_[static_cast<int>(Features)] =
             new WidgetOfFeature<Features>(component_, this),
         0)...};
    for (auto feature : features_) {
      appendSettingWidget(feature);
    }
  }

  virtual ~ComponentSettingsDialogWithFeatures() {}

private:
  std::array<QWidget *, kFeaturesNum> features_ = {nullptr};
};

} // namespace rabbit_App::component

#endif // COMPONENT_SETTINGS_DIALOG_H
