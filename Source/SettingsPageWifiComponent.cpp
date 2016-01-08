#include "SettingsPageWifiComponent.h"
#include "Main.h"
#include "Utils.h"

WifiAccessPointListItem::WifiAccessPointListItem(WifiAccessPoint *ap, WifiIcons *icons)
: Button{ ap->ssid }, ap{ ap }, icons{ icons } {}

void WifiAccessPointListItem::paintButton(Graphics &g, bool isMouseOverButton, bool isButtonDown) {
  auto bounds = getLocalBounds();
  auto inset = bounds.reduced(6, 4);
  auto w = bounds.getWidth(), h = bounds.getHeight();
  auto iconBounds = Rectangle<float>(w - h, h/5.0, h*0.6, h*0.6);
//  auto contentHeight = h * 0.7f;

  auto listOutline = Path();
  listOutline.addRoundedRectangle(inset.toFloat(), 20.0f);
  g.setColour(findColour(ListBox::ColourIds::backgroundColourId));
  g.fillPath(listOutline);

  icons->wifiStrength[ap->signalStrength]->drawWithin(g, iconBounds,
                                                      RectanglePlacement::fillDestination, 1.0f);
  if (ap->requiresAuth) {
    iconBounds.translate(-h * 0.75, 0);
    icons->lockIcon->drawWithin(g, iconBounds, RectanglePlacement::fillDestination, 1.0f);
  }

//  icons->arrowIcon->setSize(h, h);
//  icons->arrowIcon->drawWithin(g, Rectangle<float>(w - (h/8), contentHeight + 8, contentHeight, contentHeight),
//                               RectanglePlacement::fillDestination, 1.0f);

  g.setFont(Font(getLookAndFeel().getTypefaceForFont(Font())));
  g.setFont(h * 0.5);
  g.setColour(findColour(ListBox::ColourIds::textColourId));
  g.drawText(getName(), inset.reduced(h * 0.3, 0), Justification::centredLeft);
}

SettingsPageWifiComponent::SettingsPageWifiComponent() {
  pageStack = new PageStackComponent();
  addAndMakeVisible(pageStack);

  wifiIconComponent = new ImageComponent("WiFi Icon");
  wifiIconComponent->setImage(
      ImageFileFormat::loadFrom(BinaryData::wifiIcon_png, BinaryData::wifiIcon_pngSize));
  addAndMakeVisible(wifiIconComponent);

  icons = new WifiIcons();

  icons->lockIcon = Drawable::createFromImageData(BinaryData::lock_png, BinaryData::lock_pngSize);

  icons->wifiStrength = OwnedArray<Drawable>();
  icons->wifiStrength.set(0, Drawable::createFromImageData(BinaryData::wifiStrength0_png,
                                                           BinaryData::wifiStrength0_pngSize));
  icons->wifiStrength.set(1, Drawable::createFromImageData(BinaryData::wifiStrength1_png,
                                                           BinaryData::wifiStrength1_pngSize));
  icons->wifiStrength.set(2, Drawable::createFromImageData(BinaryData::wifiStrength2_png,
                                                           BinaryData::wifiStrength2_pngSize));
  icons->wifiStrength.set(3, Drawable::createFromImageData(BinaryData::wifiStrength3_png,
                                                           BinaryData::wifiStrength3_pngSize));

  icons->arrowIcon = Drawable::createFromImageData(BinaryData::backIcon_png, BinaryData::backIcon_pngSize);
  auto xf = AffineTransform::identity.rotated(M_PI);
  icons->arrowIcon->setTransform(xf);


  // create back button
  ScopedPointer<Drawable> backButtonDrawable =
      Drawable::createFromImageData(BinaryData::backIcon_png, BinaryData::backIcon_pngSize);
  backButton = createImageButtonFromDrawable("Back", *backButtonDrawable);
  backButton->addListener(this);
  backButton->setAlwaysOnTop(true);
  addAndMakeVisible(backButton);

  // create ssid list
  accessPointListPage = new TrainComponent();
  accessPointListPage->setOrientation(TrainComponent::kOrientationVertical);
  accessPointListPage->itemHeight = 50;
  accessPointListPage->itemScaleMin = accessPointListPage->itemScaleMax = 1.0;

  for (auto ap : getWifiStatus().accessPoints) {
    auto item = new WifiAccessPointListItem(ap, icons);
    item->addListener(this);
    accessPointItems.add(item);
    accessPointListPage->addItem(item);
  }

  // create connection "page"
  connectionPage = new Component("Connection Page");

  connectionLabel = new Label("Connected", "Connection Label");
  connectionLabel->setJustificationType(juce::Justification::centred);
  connectionPage->addAndMakeVisible(connectionLabel);

  passwordEditor = new TextEditor("Password", (juce_wchar)0x2022);
  passwordEditor->setText("Password");
  connectionPage->addAndMakeVisible(passwordEditor);

  connectionButton = new TextButton("Connection Button");
  connectionButton->setButtonText("Connect");
  connectionButton->addListener(this);
  connectionPage->addAndMakeVisible(connectionButton);
}

SettingsPageWifiComponent::~SettingsPageWifiComponent() {}

void SettingsPageWifiComponent::paint(Graphics &g) {}

void SettingsPageWifiComponent::setWifiEnabled(bool enabled) {
  pageStack->setVisible(enabled);
  if (enabled && pageStack->getCurrentPage() != connectionPage) {
    pageStack->clear(PageStackComponent::kTransitionNone);
    Component *nextPage = getWifiStatus().connected ? connectionPage : accessPointListPage;
    pageStack->pushPage(nextPage, PageStackComponent::kTransitionNone);
  }
}

void SettingsPageWifiComponent::resized() {
  auto bounds = getLocalBounds();
  auto pageBounds = Rectangle<int>(120, 0, bounds.getWidth() - 120, bounds.getHeight());

  pageStack->setBounds(pageBounds);

  connectionLabel->setBounds(10, 50, pageBounds.getWidth() - 20, 50);
  passwordEditor->setBounds(90, 100, pageBounds.getWidth() - 180, 50);
  connectionButton->setBounds(90, 160, pageBounds.getWidth() - 180, 50);
  wifiIconComponent->setBounds(10, 10, 60, 60);
  backButton->setBounds(bounds.getX(), bounds.getY(), 60, bounds.getHeight());

  if (!init) { // TODO: cruft to resize page correctly on init? arrg. Should be in constructor,
               //  or not at all
    init = true;
    pageStack->pushPage(accessPointListPage, PageStackComponent::kTransitionNone);
  }
}

void SettingsPageWifiComponent::buttonClicked(Button *button) {

  auto &status = getWifiStatus();

  if (button == connectionButton) {
    if (status.connected && selectedAp == status.connectedAccessPoint) {
      connectionButton->setButtonText("Connect");
      passwordEditor->setVisible(status.connectedAccessPoint->requiresAuth);
      getWifiStatus().setDisconnected();
      pageStack->popPage(PageStackComponent::kTransitionTranslateHorizontal);
    } else {
      passwordEditor->setVisible(false);
      connectionButton->setButtonText("Disconnect");
      status.setConnectedAccessPoint(selectedAp);
    }
  }

  auto apButton = dynamic_cast<WifiAccessPointListItem *>(button);
  if (apButton) {
    selectedAp = apButton->ap;
    connectionLabel->setText(apButton->ap->ssid, juce::NotificationType::dontSendNotification);
    if (selectedAp == status.connectedAccessPoint) {
      connectionButton->setButtonText("Disconnect");
    } else {
      passwordEditor->setVisible(apButton->ap->requiresAuth);
      connectionButton->setButtonText("Connect");
    }
    pageStack->pushPage(connectionPage, PageStackComponent::kTransitionTranslateHorizontal);
  }

  if (button == backButton) {
    if (pageStack->getDepth() > 1 && !status.connected) {
      pageStack->popPage(PageStackComponent::kTransitionTranslateHorizontal);
    } else {
      getMainStack().popPage(PageStackComponent::kTransitionTranslateHorizontal);
    }
  }
}
