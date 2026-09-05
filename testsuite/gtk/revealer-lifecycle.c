#include <gtk/gtk.h>

typedef struct
{
  GtkWidget *window;
  GtkWidget *revealer;
  GtkWidget *entry;
} Fixture;

static Fixture
create_fixture (gboolean with_child)
{
  Fixture fixture;

  fixture.window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  fixture.revealer = gtk_revealer_new ();
  fixture.entry = NULL;
  gtk_container_add (GTK_CONTAINER (fixture.window), fixture.revealer);
  gtk_revealer_set_transition_duration (GTK_REVEALER (fixture.revealer), 250);
  if (with_child)
    {
      fixture.entry = gtk_entry_new ();
      gtk_container_add (GTK_CONTAINER (fixture.revealer), fixture.entry);
    }
  gtk_widget_show_all (fixture.window);
  gtk_test_widget_wait_for_draw (fixture.window);
  g_assert_true (gtk_widget_get_mapped (fixture.revealer));

  return fixture;
}

static void
assert_usable (Fixture *fixture)
{
  g_assert_true (gtk_widget_get_child_visible (fixture->entry));
  g_assert_true (gtk_widget_get_realized (fixture->entry));
  g_assert_true (gtk_widget_get_mapped (fixture->entry));
  gtk_widget_grab_focus (fixture->entry);
  g_assert_true (gtk_window_get_focus (GTK_WINDOW (fixture->window)) == fixture->entry);
}

static gboolean
timed_out (gpointer data)
{
  g_error ("Revealer did not finish its transition");
  return G_SOURCE_REMOVE;
}

static void
wait_for_transition (Fixture *fixture,
                     gboolean reveal)
{
  guint timeout = g_timeout_add_seconds (5, timed_out, NULL);

  while (gtk_revealer_get_child_revealed (GTK_REVEALER (fixture->revealer)) != reveal)
    g_main_context_iteration (NULL, TRUE);

  g_source_remove (timeout);
}

static void
reveal_start (void)
{
  Fixture fixture = create_fixture (TRUE);

  g_assert_false (gtk_widget_get_child_visible (fixture.entry));
  gtk_revealer_set_reveal_child (GTK_REVEALER (fixture.revealer), TRUE);
  /* No main-context iteration: input can follow the reveal immediately. */
  assert_usable (&fixture);
  g_assert_false (gtk_revealer_get_child_revealed (GTK_REVEALER (fixture.revealer)));

  wait_for_transition (&fixture, TRUE);
  gtk_revealer_set_reveal_child (GTK_REVEALER (fixture.revealer), FALSE);
  /* Starting a hide must not unmap a child that is still on screen. */
  assert_usable (&fixture);
  wait_for_transition (&fixture, FALSE);
  g_assert_false (gtk_widget_get_child_visible (fixture.entry));
  g_assert_false (gtk_widget_get_mapped (fixture.entry));

  gtk_revealer_set_reveal_child (GTK_REVEALER (fixture.revealer), TRUE);
  assert_usable (&fixture);
  g_assert_false (gtk_revealer_get_child_revealed (GTK_REVEALER (fixture.revealer)));
  gtk_widget_destroy (fixture.window);
}

static void
reverse_before_first_frame (void)
{
  Fixture fixture = create_fixture (TRUE);

  gtk_revealer_set_reveal_child (GTK_REVEALER (fixture.revealer), TRUE);
  assert_usable (&fixture);
  gtk_revealer_set_reveal_child (GTK_REVEALER (fixture.revealer), FALSE);
  g_assert_false (gtk_widget_get_child_visible (fixture.entry));
  g_assert_false (gtk_revealer_get_child_revealed (GTK_REVEALER (fixture.revealer)));
  gtk_revealer_set_reveal_child (GTK_REVEALER (fixture.revealer), TRUE);
  assert_usable (&fixture);
  wait_for_transition (&fixture, TRUE);
  gtk_widget_destroy (fixture.window);
}

static void
reverse_during_hide (void)
{
  Fixture fixture = create_fixture (TRUE);
  guint timeout;
  gdouble opacity;

  gtk_revealer_set_transition_type (GTK_REVEALER (fixture.revealer),
                                    GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
  gtk_revealer_set_reveal_child (GTK_REVEALER (fixture.revealer), TRUE);
  wait_for_transition (&fixture, TRUE);
  gtk_revealer_set_reveal_child (GTK_REVEALER (fixture.revealer), FALSE);
  timeout = g_timeout_add_seconds (5, timed_out, NULL);
  do
    {
      g_main_context_iteration (NULL, TRUE);
      opacity = gtk_widget_get_opacity (fixture.revealer);
    }
  while (opacity == 1.0);
  g_source_remove (timeout);
  g_assert_cmpfloat (opacity, >, 0.0);

  gtk_revealer_set_reveal_child (GTK_REVEALER (fixture.revealer), TRUE);
  assert_usable (&fixture);
  /* Reversal changes the target, not the current animation position. */
  g_assert_cmpfloat (gtk_widget_get_opacity (fixture.revealer), ==, opacity);
  g_assert_false (gtk_revealer_get_child_revealed (GTK_REVEALER (fixture.revealer)));
  wait_for_transition (&fixture, TRUE);
  gtk_widget_destroy (fixture.window);
}

static void
add_after_reveal (void)
{
  Fixture fixture = create_fixture (FALSE);

  gtk_revealer_set_reveal_child (GTK_REVEALER (fixture.revealer), TRUE);
  fixture.entry = gtk_entry_new ();
  gtk_widget_show (fixture.entry);
  gtk_container_add (GTK_CONTAINER (fixture.revealer), fixture.entry);
  assert_usable (&fixture);
  g_assert_false (gtk_revealer_get_child_revealed (GTK_REVEALER (fixture.revealer)));
  gtk_widget_destroy (fixture.window);
}

int
main (int argc, char **argv)
{
  gtk_test_init (&argc, &argv, NULL);
  g_object_set (gtk_settings_get_default (), "gtk-enable-animations", TRUE, NULL);
  g_test_add_func ("/revealer/lifecycle/reveal-start", reveal_start);
  g_test_add_func ("/revealer/lifecycle/reverse-before-first-frame", reverse_before_first_frame);
  g_test_add_func ("/revealer/lifecycle/reverse-during-hide", reverse_during_hide);
  g_test_add_func ("/revealer/lifecycle/add-after-reveal", add_after_reveal);
  return g_test_run ();
}
