/*
 *  Copyright (C) 2021 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "StateColorPalette.h"

#include "gui/Application.h"

StateColorPalette::StateColorPalette()
{
    if (kpxcApp->isDarkTheme()) {
        initDefaultPaletteDark();
    } else {
        initDefaultPaletteLight();
    }
}

void StateColorPalette::initDefaultPaletteLight()
{
    setColor(ColorRole::Error, QStringLiteral("#FF7D7D"));
    setColor(ColorRole::Warning, QStringLiteral("#FFD30F"));
    setColor(ColorRole::Info, QStringLiteral("#84D0E1"));
    setColor(ColorRole::Incomplete, QStringLiteral("#FFD30F"));

    setColor(ColorRole::HealthCritical, QStringLiteral("#C43F31"));
    setColor(ColorRole::HealthBad, QStringLiteral("#E07F16"));
    setColor(ColorRole::HealthWeak, QStringLiteral("#FFD30F"));
    setColor(ColorRole::HealthOk, QStringLiteral("#FF9900"));
    setColor(ColorRole::HealthExcellent, QStringLiteral("#E08600"));

    setColor(ColorRole::True, QStringLiteral("#FF9900"));
    setColor(ColorRole::False, QStringLiteral("#C43F31"));
}

void StateColorPalette::initDefaultPaletteDark()
{
    setColor(ColorRole::Error, QStringLiteral("#802D2D"));
    setColor(ColorRole::Warning, QStringLiteral("#73682E"));
    setColor(ColorRole::Info, QStringLiteral("#207183"));
    setColor(ColorRole::Incomplete, QStringLiteral("#665124"));

    setColor(ColorRole::HealthCritical, QStringLiteral("#C43F31"));
    setColor(ColorRole::HealthBad, QStringLiteral("#DB9837"));
    setColor(ColorRole::HealthWeak, QStringLiteral("#F0C400"));
    setColor(ColorRole::HealthOk, QStringLiteral("#FFaa33"));
    setColor(ColorRole::HealthExcellent, QStringLiteral("#CC7A00"));

    setColor(ColorRole::True, QStringLiteral("#FFaa33"));
    setColor(ColorRole::False, QStringLiteral("#C43F31"));
}
