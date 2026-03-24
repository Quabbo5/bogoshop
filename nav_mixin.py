from tkinter import *


class NavMixin:

    def _nav_push(self):
        """Push current view onto the back-stack before navigating away."""
        if self._current_view is not None:
            self._nav_stack.append(self._current_view)

    def _nav_back(self):
        """Go back to the previous panel, or community if history is empty."""
        if self._nav_stack:
            self._current_view = None       # prevent re-push inside the popped fn
            fn = self._nav_stack.pop()
            fn()
        else:
            self._show_community_panel()
