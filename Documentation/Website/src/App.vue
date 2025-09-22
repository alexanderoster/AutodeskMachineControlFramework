
<template>
  <v-app>
    <v-main>
      <v-container class="py-12">
        <v-card class="pa-6">
          <h1 class="text-h4 mb-2">Vuetify + Vite + GitHub Pages</h1>
          <p class="text-body-1">Deployed with Actions.</p>
          <v-btn prepend-icon="mdi-github">View on GitHub</v-btn>
        </v-card>
      </v-container>
    </v-main>
  </v-app>
</template>

<style scoped>
header {
  line-height: 1.5;
}

.logo {
  display: block;
  margin: 0 auto 2rem;
}

@media (min-width: 1024px) {
  header {
    display: flex;
    place-items: center;
    padding-right: calc(var(--section-gap) / 2);
  }

  .logo {
    margin: 0 2rem 0 0;
  }

  header .wrapper {
    display: flex;
    place-items: flex-start;
    flex-wrap: wrap;
  }
}
</style>
