//! The `microts::Host` of every generated app.

use microts::{HasButton, HasButtons, HasMotion, HasRelativeAxis, HasTouch, Host, Ui};

/// Owns the retained UI and admits every input channel a generated view may
/// demand: the C input source and further sources decide what the board
/// actually delivers.
pub struct M5Host(pub Ui);

impl Host for M5Host {
    fn ui(&self) -> &Ui {
        &self.0
    }
    fn ui_mut(&mut self) -> &mut Ui {
        &mut self.0
    }
    fn into_ui(self) -> Ui {
        self.0
    }
}
impl HasButtons for M5Host {}
impl<const MASK: u32> HasButton<MASK> for M5Host {}
impl HasTouch for M5Host {}
impl<const AXIS: u8> HasRelativeAxis<AXIS> for M5Host {}
impl<const LEVEL: u8> HasMotion<LEVEL> for M5Host {}
