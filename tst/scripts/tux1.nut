wait(0.5);
Tux.walk(200);
wait(1);
Tux.walk(0);
Tux.do_jump(-400);
wait(0.3);
Tux.deactivate();
wait(1);
Tux.activate();
Tux.set_velocity(100, -300);
wait(0.75);
if (Tux.get_velocity_x() > 50) {
  Tux.walk(-150);
} else {
  Tux.walk(150);
}
wait(1.25);
state.n <- 3;
if (state.n == 3 && !("m" in state)) {
  Tux.do_jump(-200);
}
wait(2);
Tux.kill(false);
